#include <gtest/gtest.h>

#include <cstring>
#include <fstream>
#include <memory>
#include <thread>

#include "config/config_manager.hpp"
#include "core/simulator.hpp"
#include "util/queue.hpp"

extern std::string config_file_name;
using namespace std::chrono_literals;

namespace {

class V3TestProj : public ::testing::Test {
 protected:
  void SetUp() override {
    std::ifstream f(config_file_name);
    f >> config_json_;
  }

  nlohmann::json config_json_;
};


// For test: capture the batch's raw ray segments.
//
// This used to be an IConsume registered on the consumer thread, reading
// SimData::rays_. That buffer no longer travels on SimData — the legacy CPU
// path recycles it inside Simulator::SimWorkspace, so the only remaining seat
// for a content read is the simulator's own test observer, which fires
// synchronously on the producer thread just before the batch is queued. Same
// batch, same rays, same selection rule; only the seat moved.
void CopyRayData(void* ctx, const lumice::RayBuffer& all_data) {
  auto* output_data = static_cast<float*>(ctx);
  int offset = 0;
  for (const auto& r : all_data) {
    if (r.to_face_ != lumice::kInvalidId || r.w_ < 0) {
      continue;
    }
    std::memcpy(output_data + offset * 7 + 0, r.p_, 3 * sizeof(float));
    std::memcpy(output_data + offset * 7 + 3, r.d_, 3 * sizeof(float));
    output_data[offset * 7 + 6] = r.w_;
    offset++;
  }
}

// Queue drain. It registers no IConsume any more — the batch's ray content is
// captured by the simulator-side observer above — but the produced SimData
// still has to be taken off the queue and the loop's exit condition still
// exercises the "no ray segments" sentinel the server reads.
class Consumer {
 public:
  Consumer(lumice::QueuePtrS<lumice::SimData> data_queue) : data_queue_(data_queue), stop_(false) {}

  void Run() {
    while (true) {
      auto data = data_queue_->Get();
      if (stop_ || data.ray_seg_count_ == 0) {
        break;
      }
    }
  }

  void Stop() { stop_ = true; }

 private:
  lumice::QueuePtrS<lumice::SimData> data_queue_;
  std::atomic_bool stop_;
};

TEST_F(V3TestProj, SimpleProj) {
  lumice::ConfigManager config_manager = config_json_.get<lumice::ConfigManager>();

  auto config_queue = std::make_shared<lumice::Queue<lumice::SimBatch>>();
  auto data_queue = std::make_shared<lumice::Queue<lumice::SimData>>();

  constexpr int kMaxHits = 8;
  auto output_data = std::make_unique<float[]>(kMaxHits * 2 * 7);

  constexpr uint32_t kTestSeed = 45;
  lumice::Simulator simulator(config_queue, data_queue, kTestSeed);

  simulator.SetAllDataObserverForTest(&CopyRayData, output_data.get());

  // Still drained on its own thread: the batch must actually be produced and
  // published for the observer above to have fired on a complete batch.
  Consumer consumer(data_queue);

  std::thread prod_thread([&simulator]() { simulator.Run(); });
  std::thread cons_thread([&consumer]() { consumer.Run(); });

  const auto& config = config_manager.scene_;
  auto scene_ptr = std::make_shared<const lumice::SceneConfig>(config);
  config_queue->Emplace(lumice::SimBatch{ config.ray_num_, scene_ptr, 0 });

  std::this_thread::sleep_for(500ms);
  simulator.Stop();
  consumer.Stop();

  cons_thread.join();
  prod_thread.join();

  // Fixture regenerated when InitRay_p_fid gained the projected-area
  // acceptance (each entry ray kept with probability A(o,g,d) / (S/2)). That
  // acceptance draws one uniform per ray, so the stream moved; and at the old
  // seed 42 both of this config's 2 rays are now discarded, which leaves the
  // capture all zeros — a fixture that compares nothing. Seed 45 was picked
  // because both rays enter the crystal there. Captured from
  // test/fixtures/v3_config.json (crystal id=1, default axis = canonical pose);
  // the two rays' segments interleave in hit order, as before. Physics
  // signatures to read it by: p[0] = ±0.433013 (±√3/4) on the ±x prism side
  // faces, and the Fresnel weight decaying along each ray's history.
  float expect_out[kMaxHits * 2 * 7]{
    /* --------- p --------------->|<-------------- d ------------->|<-- w -->|*/
    0.433013f,  0.232729f,  -0.548703f, 0.939850f,  0.000164f,  -0.341588f, 0.018279f,  //
    0.228281f,  0.368202f,  0.544418f,  -0.470604f, 0.812336f,  -0.344443f, 0.064894f,  //
    -0.433013f, 0.127994f,  0.352839f,  -0.833574f, -0.431873f, -0.344443f, 0.916629f,  //
    -0.433013f, 0.232841f,  -0.417579f, -0.939850f, 0.000164f,  0.341588f,  0.963776f,  //
    0.433013f,  -0.186580f, 0.101948f,  0.833573f,  -0.431873f, -0.344443f, 0.018112f,  //
    0.433013f,  0.232953f,  -0.183861f, 0.939850f,  0.000164f,  0.341588f,  0.017617f,  //
    -0.165995f, -0.404163f, -0.071587f, -0.938806f, 0.001387f,  -0.344443f, 0.000341f,  //
    -0.433013f, 0.233065f,  0.049857f,  -0.939850f, 0.000164f,  0.341588f,  0.000322f,  //
    -0.295799f, 0.329221f,  -0.274389f, -0.042774f, 0.937832f,  -0.344443f, 0.000023f,  //
    0.433013f,  0.233178f,  0.283575f,  0.939850f,  0.000164f,  0.341588f,  0.000006f,  //
    0.409643f,  -0.263492f, -0.525280f, 0.790799f,  -0.505959f, -0.344443f, 0.000000f,  //
    -0.433013f, 0.233290f,  0.517293f,  -0.939850f, 0.000164f,  0.341588f,  0.000000f,  //
    0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  //
    0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  //
    0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  //
    0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  //
  };

  for (int i = 0; i < kMaxHits * 2; i++) {
    for (int j = 0; j < 7; j++) {
      EXPECT_NEAR(expect_out[i * 7 + j], output_data[i * 7 + j], 5e-5);
    }
  }
}

}  // namespace
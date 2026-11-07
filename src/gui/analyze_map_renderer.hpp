#ifndef LUMICE_GUI_ANALYZE_MAP_RENDERER_HPP
#define LUMICE_GUI_ANALYZE_MAP_RENDERER_HPP

#include <cstddef>
#include <optional>

#include "gui/analyze_map_view.hpp"

namespace lumice::gui {

struct ViewProjection;

// A reading owns its source for the entire borrowed-column lifetime. Unknown has no reading;
// finite zero and singular each retain their original value/count/status instead of a colour.
struct MapCellReading {
  MapHandle source;
  LUMICE_PathMapInfo info{};
  LUMICE_PathMapPixels pixels{};
  size_t index = 0;
  bool detail = false;
};
std::optional<size_t> MapCellIndex(const LUMICE_PathMapInfo& info, const double travel[3]);
std::optional<MapCellReading> ReadFrozenMapDirection(const MapQuerySession& map, const double travel[3]);
std::optional<std::array<double, 3>> FrozenMapFarTravel(const ViewProjection& view, int width, int height, int px,
                                                        int py);

struct MapDisplayFrame {
  unsigned int texture = 0;
  int width = 0;
  int height = 0;
  bool display_sampled = false;
  bool compatible = false;
  bool partial = false;
  int members_evaluated = 0;
  int member_count = 0;
  MapFitResult fit;
  double adoption_ms = 0.0;
  SurveyQueryId source_adopted_id;
  double observation_ms = 0.0;
  size_t source_gpu_bytes = 0;
  size_t source_gpu_peak_bytes = 0;
  size_t sort_peak_bytes = 0;
  size_t target_gpu_bytes = 0;
  size_t staging_peak_bytes = 0;
};

// GL-context thread only. Uses the same projection GLSL as the main picture. Sources upload
// only on result identity changes; observations change uniforms and draw, never raw columns.
// Optional caller-owned FBO readback for direction/source/index/status and floating colour.
// Normal display does not allocate these attachments or retain any readback arrays.
struct MapDisplayProbe {
  std::vector<std::array<float, 4>> near_cells;
  std::vector<std::array<float, 4>> far_cells;
  std::vector<std::array<float, 4>> color;
};
MapDisplayFrame DrawFrozenMap(const MapQuerySession& map, const ViewProjection& view, int width, int height,
                              MapDisplayProbe* probe = nullptr);
bool InitializeFrozenMapDisplay();
bool PrepareFrozenMapSource(const MapHandle& source, SurveyQueryId id, bool detail);
const ViewProjection& LastFrozenMapView();
MapDisplayFrame LastFrozenMapAdoption();
void PruneFrozenMapDisplay(const MapQuerySession& map);
void DestroyFrozenMapDisplay();

}  // namespace lumice::gui

#endif

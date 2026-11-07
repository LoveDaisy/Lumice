#include "gui/analyze_map_renderer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <vector>

#include "gui/analyze_map_view.hpp"
#include "gui/gl_common.h"
#include "gui/gui_logger.hpp"
#include "gui/preview_renderer.hpp"

namespace lumice::gui {
namespace {
constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
using Clock = std::chrono::steady_clock;
double Milliseconds(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

bool ReadSource(const MapHandle& source, LUMICE_PathMapInfo* info, LUMICE_PathMapPixels* pixels) {
  info->struct_size = sizeof(*info);
  pixels->struct_size = sizeof(*pixels);
  return source && LUMICE_PathMapGetInfo(source.get(), info) == LUMICE_PATH_READ_OK &&
         info->state == LUMICE_PATH_STATE_FINITE &&
         LUMICE_PathMapGetPixels(source.get(), pixels) == LUMICE_PATH_READ_OK;
}

// No borrowed pointers are read after upload. IDs rather than heap addresses bind the cache.
struct SourceTexture {
  unsigned int texture = 0;
  SurveyQueryId id;
  LUMICE_PathMapInfo info{};
  MapFitResult fit;
  size_t bytes = 0;
};
struct DisplayCache {
  SourceTexture reference;
  SourceTexture detail;
  unsigned int program = 0;
  unsigned int vao = 0;
  unsigned int target = 0;
  unsigned int fbo = 0;
  int width = 0;
  int height = 0;
};
DisplayCache g_display;
ViewProjection g_last_view;
MapDisplayFrame g_last_adoption;

// Transfer state belongs to the caller, not the map. In particular an XYZ PBO or non-zero
// row skip from another producer must never change how a frozen source is uploaded/read.
struct ScopedMapTransferState {
  int active = 0, texture[2]{}, unpack = 0, pack = 0;
  int unpack_alignment = 0, unpack_row = 0, unpack_skip_rows = 0, unpack_skip_pixels = 0;
  int pack_alignment = 0, pack_row = 0, pack_skip_rows = 0, pack_skip_pixels = 0;
  bool depth = false, cull = false;
  ScopedMapTransferState() {
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    for (int i = 0; i < 2; ++i) {
      glActiveTexture(GL_TEXTURE0 + i);
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture[i]);
    }
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpack_row);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &unpack_skip_rows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &unpack_skip_pixels);
    glGetIntegerv(GL_PACK_ALIGNMENT, &pack_alignment);
    glGetIntegerv(GL_PACK_ROW_LENGTH, &pack_row);
    glGetIntegerv(GL_PACK_SKIP_ROWS, &pack_skip_rows);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &pack_skip_pixels);
    depth = glIsEnabled(GL_DEPTH_TEST);
    cull = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
  }
  ~ScopedMapTransferState() {
    for (int i = 0; i < 2; ++i) {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_2D, glIsTexture(texture[i]) ? texture[i] : 0);
    }
    glActiveTexture(active);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pack);
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, unpack_row);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, unpack_skip_rows);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, unpack_skip_pixels);
    glPixelStorei(GL_PACK_ALIGNMENT, pack_alignment);
    glPixelStorei(GL_PACK_ROW_LENGTH, pack_row);
    glPixelStorei(GL_PACK_SKIP_ROWS, pack_skip_rows);
    glPixelStorei(GL_PACK_SKIP_PIXELS, pack_skip_pixels);
    if (depth)
      glEnable(GL_DEPTH_TEST);
    if (cull)
      glEnable(GL_CULL_FACE);
  }
};

void DestroySource(SourceTexture& source) {
  if (source.texture) {
    glDeleteTextures(1, &source.texture);
  }
  source = {};
}

// clang-format off
const char* kVertex = R"glsl(#version 330 core
out vec2 v_ndc;
void main() {
  vec2 p = gl_VertexID == 0 ? vec2(-1,-1) : (gl_VertexID == 1 ? vec2(3,-1) : vec2(-1,3));
  v_ndc = p;
  gl_Position = vec4(p,0,1);
}
)glsl";
const char* kMapUniforms = R"glsl(#version 330 core
#define LUMICE_GLOBE_BACK_FADE 1
in vec2 v_ndc;
uniform vec2 u_resolution;
uniform int u_lens_type;
uniform float u_fov;
uniform mat3 u_view_matrix;
uniform int u_visible;
uniform int u_front;
uniform float u_globe_back_fade;
const float PI = 3.14159265358979323846;
)glsl";
const char* kMapFragment = R"glsl(
uniform sampler2D u_reference;
uniform sampler2D u_detail;
uniform mat3 u_reference_frame;
uniform mat3 u_detail_frame;
uniform vec4 u_reference_bounds;
uniform vec4 u_detail_bounds;
uniform ivec2 u_reference_size;
uniform ivec2 u_detail_size;
uniform int u_have_reference;
uniform int u_have_detail;
uniform int u_source_mode;
uniform int u_compatible;
layout(location=0) out vec4 frag_color;
layout(location=1) out vec4 probe_near;
layout(location=2) out vec4 probe_far;
layout(location=3) out vec4 probe_color;

bool cellAt(vec3 d, mat3 frame, vec4 bounds, ivec2 size, out ivec2 cell) {
  if (size.x <= 0 || size.y <= 0) return false;
  float delta = degrees(atan(length(cross(d, frame[0])), dot(d, frame[0])));
  float az = degrees(atan(dot(d,frame[2]), dot(d,frame[1])));
  az = bounds.z + mod(az-bounds.z, 360.0);
  if (delta < bounds.x || delta > bounds.y || az > bounds.w) return false;
  vec2 uv = (vec2(az,delta)-vec2(bounds.z,bounds.x)) / vec2(bounds.w-bounds.z,bounds.y-bounds.x);
  cell = clamp(ivec2(floor(uv*vec2(size))), ivec2(0), size-ivec2(1));
  return true;
}
ivec2 packedIndex(int index, int width) { return ivec2(index % width,index / width); }
vec4 sampleMap(vec3 d, out vec3 identity) {
  ivec2 cell;
  vec4 value;
  identity = vec3(0,-1,0);
  bool detail_selected = u_source_mode == 2 || (u_source_mode == 0 && (u_compatible != 0 || u_have_reference == 0));
  if (detail_selected && u_have_detail != 0 && cellAt(d,u_detail_frame,u_detail_bounds,u_detail_size,cell)) {
    value = texelFetch(u_detail,packedIndex(cell.y*u_detail_size.x+cell.x,textureSize(u_detail,0).x),0);
    identity = vec3(2,cell.y*u_detail_size.x+cell.x,value.a);
    return value;
  }
  if (u_source_mode != 2 && u_have_reference != 0 &&
      cellAt(d,u_reference_frame,u_reference_bounds,u_reference_size,cell)) {
    value = texelFetch(u_reference,packedIndex(cell.y*u_reference_size.x+cell.x,textureSize(u_reference,0).x),0);
    identity = vec3(1,cell.y*u_reference_size.x+cell.x,value.a);
    return value;
  }
  return vec4(0); // unknown, NOT a finite-zero cell
}
vec3 statusColour(vec4 cell, vec2 pos) {
  if (cell.a < 0.5) {
    float stripe = mod(floor((pos.x+pos.y)/6.0),2.0);
    return mix(vec3(0.10,0.105,0.12),vec3(0.17,0.175,0.19),stripe);
  }
  if (cell.a > 1.5 && cell.a < 2.5) return vec3(0.42,0.22,0.43); // singular, not finite
  return cell.rgb;
}
void main() {
  vec2 pos = v_ndc*u_resolution*0.5;
  float half_fov = radians(u_fov)*0.5;
  float ri; vec2 pos_ovl;
  vec4 near_dir = inverseWorldDir(pos,half_fov,ri,pos_ovl);
  bool near_visible = near_dir.w > 0.5 && dirPassesClip(near_dir.xyz);
  vec3 colour = vec3(0.055,0.06,0.075);
  probe_near = vec4(0,-1,0,0);
  probe_far = vec4(0,-1,0,0);
  vec3 identity;
  if (near_visible) {
    colour = statusColour(sampleMap(near_dir.xyz,identity),pos);
    probe_near = vec4(identity,1);
  }
  if (u_lens_type == 10 && u_globe_back_fade > 0.0) {
    float weight; float far_ri;
    vec4 far_dir = globeFarDir(pos,half_fov,far_ri,weight);
    if (far_dir.w > 0.5 && dirPassesClip(far_dir.xyz)) {
      // A visual hint of a different sky direction, never a per-sr sum or a raw reading.
      vec3 far_colour = statusColour(sampleMap(far_dir.xyz,identity),pos);
      probe_far = vec4(identity,weight);
      colour = mix(colour,far_colour,0.35*weight);
    }
  }
  frag_color = vec4(colour,1);
  probe_color = frag_color;
}
)glsl";
// clang-format on

unsigned int Compile(unsigned int type, const std::vector<const char*>& parts) {
  const unsigned int shader = glCreateShader(type);
  glShaderSource(shader, static_cast<int>(parts.size()), parts.data(), nullptr);
  glCompileShader(shader);
  int ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[2048]{};
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    GUI_LOG_ERROR("Map shader: {}", log);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

bool InitProgram() {
  if (g_display.program) {
    return true;
  }
  const unsigned int vs = Compile(GL_VERTEX_SHADER, { kVertex });
  const unsigned int fs = Compile(GL_FRAGMENT_SHADER, { kMapUniforms, PreviewProjectionGlsl(), kMapFragment });
  if (!vs || !fs) {
    if (vs)
      glDeleteShader(vs);
    if (fs)
      glDeleteShader(fs);
    return false;
  }
  const unsigned int program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  glDeleteShader(vs);
  glDeleteShader(fs);
  int ok = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[2048]{};
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    GUI_LOG_ERROR("Map program: {}", log);
    glDeleteProgram(program);
    return false;
  }
  g_display.program = program;
  glGenVertexArrays(1, &g_display.vao);
  glGenFramebuffers(1, &g_display.fbo);
  return true;
}

bool UploadSource(SourceTexture& slot, const MapHandle& source, SurveyQueryId id, MapDisplayFrame* frame) {
  if (!source) {
    DestroySource(slot);
    return true;
  }
  if (slot.texture && slot.id == id) {
    return true;
  }
  const auto start = Clock::now();
  LUMICE_PathMapInfo info{};
  LUMICE_PathMapPixels pixels{};
  if (!ReadSource(source, &info, &pixels) || info.view_rows <= 0 || info.view_cols <= 0 ||
      pixels.pixel_count != static_cast<size_t>(info.view_rows) * info.view_cols) {
    return false;
  }
  size_t norm_temporary = 0, fit_temporary = 0;
  const auto norm = MapDisplayNormFor(pixels, &norm_temporary);
  const auto fit = FitMapViewFromPixels(pixels, info, &fit_temporary);
  frame->sort_peak_bytes = std::max({ frame->sort_peak_bytes, norm_temporary, fit_temporary });
  // Grid dimensions are mathematical metadata, not GL allocation dimensions. A legal narrow
  // grid can have thousands of azimuth columns; pack the immutable row-major cells so every
  // <=262144-cell source also fits the minimum supported 2D texture-size guarantee.
  const int texture_width = static_cast<int>(std::min<size_t>(1024, pixels.pixel_count));
  const int texture_height = static_cast<int>((pixels.pixel_count + texture_width - 1) / texture_width);
  std::vector<std::array<float, 4>> colors(static_cast<size_t>(texture_width) * texture_height);
  frame->staging_peak_bytes = std::max(frame->staging_peak_bytes, colors.capacity() * sizeof(colors[0]));
  for (size_t i = 0; i < pixels.pixel_count; ++i) {
    const auto color = MapCellColor(pixels.values[i], pixels.status[i], norm);
    std::copy(color.begin(), color.end(), colors[i].begin());
    colors[i][3] = pixels.status[i] == LUMICE_PATH_MAP_PIXEL_SINGULAR   ? 2.0f :
                   pixels.status[i] == LUMICE_PATH_MAP_PIXEL_POINT_MASS ? 3.0f :
                                                                          1.0f;
  }
  SourceTexture prepared;
  prepared.id = id;
  prepared.info = info;
  prepared.fit = fit;
  prepared.bytes = pixels.pixel_count * sizeof(colors[0]);
  glGenTextures(1, &prepared.texture);
  glBindTexture(GL_TEXTURE_2D, prepared.texture);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texture_width, texture_height, 0, GL_RGBA, GL_FLOAT, colors.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  int allocated_width = 0, allocated_height = 0, allocated_format = 0;
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &allocated_width);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &allocated_height);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &allocated_format);
  if (allocated_width != texture_width || allocated_height != texture_height || allocated_format != GL_RGBA32F) {
    DestroySource(prepared);
    return false;
  }
  prepared.bytes = static_cast<size_t>(allocated_width) * allocated_height * 4 * sizeof(float);
  frame->source_gpu_peak_bytes =
      std::max(frame->source_gpu_peak_bytes, g_display.reference.bytes + g_display.detail.bytes + prepared.bytes);
  if (glGetError() != GL_NO_ERROR) {
    DestroySource(prepared);
    return false;
  }
  // Source adoption, not observation: finish the upload and previous source commands before
  // releasing the old resource. This also makes the <=100 ms measurement include GPU readiness.
  glFinish();
  DestroySource(slot);
  slot = prepared;
  frame->adoption_ms += Milliseconds(start);
  frame->source_adopted_id = id;
  return true;
}

void SetSourceUniforms(const SourceTexture& source, const char* frame_name, const char* bounds_name,
                       const char* size_name) {
  const auto& info = source.info;
  float matrix[9];
  for (int k = 0; k < 3; ++k) {
    matrix[k] = static_cast<float>(info.view_incident[k]);
    matrix[3 + k] = static_cast<float>(info.view_e1[k]);
    matrix[6 + k] = static_cast<float>(info.view_e2[k]);
  }
  glUniformMatrix3fv(glGetUniformLocation(g_display.program, frame_name), 1, GL_FALSE, matrix);
  glUniform4f(glGetUniformLocation(g_display.program, bounds_name), static_cast<float>(info.view_delta_min_deg),
              static_cast<float>(info.view_delta_max_deg), static_cast<float>(info.view_az_min_deg),
              static_cast<float>(info.view_az_max_deg));
  glUniform2i(glGetUniformLocation(g_display.program, size_name), source.texture ? info.view_cols : 0,
              source.texture ? info.view_rows : 0);
}

}  // namespace

std::optional<size_t> MapCellIndex(const LUMICE_PathMapInfo& info, const double travel[3]) {
  if (info.view_rows <= 0 || info.view_cols <= 0) {
    return std::nullopt;
  }
  double dot = 0.0;
  double x = 0.0;
  double y = 0.0;
  double norm = 0.0;
  for (int k = 0; k < 3; ++k) {
    dot += travel[k] * info.view_incident[k];
    x += travel[k] * info.view_e1[k];
    y += travel[k] * info.view_e2[k];
    norm += travel[k] * travel[k];
  }
  if (!(norm > 0.0)) {
    return std::nullopt;
  }
  const double delta = std::atan2(std::hypot(x, y), dot) * kRadToDeg;
  const double raw_az = std::atan2(y, x) * kRadToDeg;
  const double az = info.view_az_min_deg + std::fmod(std::fmod(raw_az - info.view_az_min_deg, 360.0) + 360.0, 360.0);
  if (delta < info.view_delta_min_deg || delta > info.view_delta_max_deg || az > info.view_az_max_deg) {
    return std::nullopt;
  }
  const int row = std::clamp(static_cast<int>((delta - info.view_delta_min_deg) /
                                              (info.view_delta_max_deg - info.view_delta_min_deg) * info.view_rows),
                             0, info.view_rows - 1);
  const int col = std::clamp(
      static_cast<int>((az - info.view_az_min_deg) / (info.view_az_max_deg - info.view_az_min_deg) * info.view_cols), 0,
      info.view_cols - 1);
  return static_cast<size_t>(row) * info.view_cols + col;
}

std::optional<MapCellReading> ReadFrozenMapDirection(const MapQuerySession& map, const double travel[3]) {
  MapCellReading reference;
  reference.source = map.adopted_result;
  const bool have_reference = ReadSource(reference.source, &reference.info, &reference.pixels);
  MapCellReading detail;
  detail.source = map.detail_result;
  detail.detail = true;
  const bool have_detail = ReadSource(detail.source, &detail.info, &detail.pixels);
  const bool composed = have_reference && have_detail && MapSourcesCompatible(reference.info, detail.info);
  if (have_detail && (map.source_mode == 2 || (map.source_mode == 0 && (composed || !have_reference)))) {
    if (const auto index = MapCellIndex(detail.info, travel)) {
      detail.index = *index;
      return detail;
    }
  }
  if (have_reference && map.source_mode != 2) {
    if (const auto index = MapCellIndex(reference.info, travel)) {
      reference.index = *index;
      return reference;
    }
  }
  return std::nullopt;
}

std::optional<std::array<double, 3>> FrozenMapFarTravel(const ViewProjection& view, int width, int height, int px,
                                                        int py) {
  if (view.lens_type != kLensTypeGlobe || !(view.globe_back_fade > 0.0f) || width <= 0 || height <= 0 || px < 0 ||
      py < 0 || px >= width || py >= height)
    return std::nullopt;
  const double x = px + 0.5 - width * 0.5, y = height * 0.5 - py - 0.5;
  const double f = std::min(width, height) * 0.5 / std::tan(static_cast<double>(view.fov) / kRadToDeg * 0.5);
  const double n = std::sqrt(x * x + y * y + f * f);
  const double dx = x / n, dy = y / n, dz = -f / n;
  const double b = 4.0 * dz, disc = b * b - 15.0;
  if (disc < 0.0)
    return std::nullopt;
  const double t = -b + std::sqrt(disc);
  const double local[3] = { t * dx, t * dy, 4.0 + t * dz };
  float matrix[9];
  BuildViewMatrix(view.elevation, view.azimuth, view.roll, matrix);
  std::array<double, 3> travel{};
  for (int k = 0; k < 3; ++k)
    travel[k] = matrix[k] * local[0] + matrix[3 + k] * local[1] + matrix[6 + k] * local[2];
  if ((view.visible == kVisibleUpper && travel[2] > 0.0) || (view.visible == kVisibleLower && travel[2] < 0.0))
    return std::nullopt;
  return travel;
}

bool InitializeFrozenMapDisplay() {
  return InitProgram();
}

bool PrepareFrozenMapSource(const MapHandle& source, SurveyQueryId id, bool detail) {
  ScopedMapTransferState transfer;
  if (!InitProgram())
    return false;
  glActiveTexture(GL_TEXTURE0);
  while (glGetError() != GL_NO_ERROR) {
  }
  MapDisplayFrame frame;
  const bool ok = UploadSource(detail ? g_display.detail : g_display.reference, source, id, &frame);
  if (ok) {
    frame.source_gpu_bytes = g_display.reference.bytes + g_display.detail.bytes;
    g_last_adoption = frame;
  }
  return ok;
}

const ViewProjection& LastFrozenMapView() {
  return g_last_view;
}
MapDisplayFrame LastFrozenMapAdoption() {
  return g_last_adoption;
}

void PruneFrozenMapDisplay(const MapQuerySession& map) {
  if (!map.adopted_result || g_display.reference.id != map.adopted_id) {
    DestroySource(g_display.reference);
  }
  if (!map.detail_result || g_display.detail.id != map.detail_id) {
    DestroySource(g_display.detail);
  }
}

void DestroyFrozenMapDisplay() {
  DestroySource(g_display.reference);
  DestroySource(g_display.detail);
  if (g_display.target)
    glDeleteTextures(1, &g_display.target);
  if (g_display.fbo)
    glDeleteFramebuffers(1, &g_display.fbo);
  if (g_display.vao)
    glDeleteVertexArrays(1, &g_display.vao);
  if (g_display.program)
    glDeleteProgram(g_display.program);
  g_display = {};
}

MapDisplayFrame DrawFrozenMap(const MapQuerySession& map, const ViewProjection& view, int width, int height,
                              MapDisplayProbe* probe) {
  MapDisplayFrame frame;
  ScopedMapTransferState transfer;
  g_last_view = view;
  if (width <= 0 || height <= 0 || !InitProgram())
    return frame;
  // Bound display-only sampling outside the measured pressure domain. Raw direction readers
  // still use the original physical canvas; this never changes a calculation target or source.
  const double scale = std::min({ 1.0, 1600.0 / width, 1200.0 / height });
  frame.width = std::max(1, static_cast<int>(std::floor(width * scale)));
  frame.height = std::max(1, static_cast<int>(std::floor(height * scale)));
  frame.display_sampled = scale < 1.0;
  int old_fbo = 0;
  int old_viewport[4];
  int old_program = 0;
  int old_vao = 0;
  int old_active = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);
  glGetIntegerv(GL_VIEWPORT, old_viewport);
  glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &old_vao);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);
  const bool blend = glIsEnabled(GL_BLEND);
  const bool scissor = glIsEnabled(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_SCISSOR_TEST);
  glActiveTexture(GL_TEXTURE0);
  while (glGetError() != GL_NO_ERROR) {
  }
  const bool sources_ok = UploadSource(g_display.reference, map.adopted_result, map.adopted_id, &frame) &&
                          UploadSource(g_display.detail, map.detail_result, map.detail_id, &frame);
  frame.compatible = g_display.reference.texture && g_display.detail.texture &&
                     MapSourcesCompatible(g_display.reference.info, g_display.detail.info);
  if (g_display.width != frame.width || g_display.height != frame.height) {
    if (g_display.target) {
      // Geometry-only replacement: do not accumulate driver-retained targets across resizes.
      glFinish();
      glDeleteTextures(1, &g_display.target);
    }
    glGenTextures(1, &g_display.target);
    glBindTexture(GL_TEXTURE_2D, g_display.target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, frame.width, frame.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, g_display.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_display.target, 0);
    g_display.width = frame.width;
    g_display.height = frame.height;
  }
  std::array<unsigned int, 3> diagnostic_textures{};
  glBindFramebuffer(GL_FRAMEBUFFER, g_display.fbo);
  if (probe) {
    glGenTextures(3, diagnostic_textures.data());
    for (size_t i = 0; i < diagnostic_textures.size(); ++i) {
      glBindTexture(GL_TEXTURE_2D, diagnostic_textures[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, frame.width, frame.height, 0, GL_RGBA, GL_FLOAT, nullptr);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1 + static_cast<unsigned int>(i), GL_TEXTURE_2D,
                             diagnostic_textures[i], 0);
    }
    const unsigned int buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
                                     GL_COLOR_ATTACHMENT3 };
    glDrawBuffers(4, buffers);
  } else {
    const unsigned int buffer = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &buffer);
  }
  const auto& selected_source =
      map.source_mode == 2 || !g_display.reference.texture ? g_display.detail : g_display.reference;
  if (selected_source.texture) {
    frame.members_evaluated = selected_source.info.members_evaluated;
    frame.member_count = selected_source.info.member_count;
    frame.partial = selected_source.info.truncated || frame.members_evaluated < frame.member_count;
    if (map.source_mode == 0 && frame.compatible)
      frame.partial = frame.partial || g_display.detail.info.truncated;
  }
  frame.fit = map.source_mode == 2 || !g_display.reference.texture ? g_display.detail.fit : g_display.reference.fit;
  const auto start = Clock::now();
  glBindFramebuffer(GL_FRAMEBUFFER, g_display.fbo);
  if (sources_ok && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
    glViewport(0, 0, frame.width, frame.height);
    glUseProgram(g_display.program);
    const auto uniform = [](const char* name) { return glGetUniformLocation(g_display.program, name); };
    float matrix[9];
    BuildViewMatrix(view.elevation, view.azimuth, view.roll, matrix);
    glUniformMatrix3fv(uniform("u_view_matrix"), 1, GL_FALSE, matrix);
    glUniform2f(uniform("u_resolution"), static_cast<float>(width), static_cast<float>(height));
    glUniform1i(uniform("u_lens_type"), view.lens_type);
    glUniform1f(uniform("u_fov"), view.fov);
    glUniform1i(uniform("u_visible"), view.visible);
    glUniform1i(uniform("u_front"), view.front ? 1 : 0);
    glUniform1f(uniform("u_globe_back_fade"), view.globe_back_fade);
    glUniform1i(uniform("u_source_mode"), map.source_mode);
    glUniform1i(uniform("u_compatible"), frame.compatible ? 1 : 0);
    glUniform1i(uniform("u_have_reference"), g_display.reference.texture ? 1 : 0);
    glUniform1i(uniform("u_have_detail"), g_display.detail.texture ? 1 : 0);
    SetSourceUniforms(g_display.reference, "u_reference_frame", "u_reference_bounds", "u_reference_size");
    SetSourceUniforms(g_display.detail, "u_detail_frame", "u_detail_bounds", "u_detail_size");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_display.reference.texture);
    glUniform1i(uniform("u_reference"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_display.detail.texture);
    glUniform1i(uniform("u_detail"), 1);
    glBindVertexArray(g_display.vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (glGetError() == GL_NO_ERROR)
      frame.texture = g_display.target;
  }
  if (probe) {
    std::vector<std::array<float, 4>>* destinations[] = { &probe->near_cells, &probe->far_cells, &probe->color };
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    for (size_t i = 0; i < diagnostic_textures.size(); ++i) {
      destinations[i]->resize(static_cast<size_t>(frame.width) * frame.height);
      glReadBuffer(GL_COLOR_ATTACHMENT1 + static_cast<unsigned int>(i));
      glReadPixels(0, 0, frame.width, frame.height, GL_RGBA, GL_FLOAT, destinations[i]->data());
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1 + static_cast<unsigned int>(i), GL_TEXTURE_2D, 0, 0);
    }
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    const unsigned int buffer = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &buffer);
    glDeleteTextures(3, diagnostic_textures.data());
    if (glGetError() != GL_NO_ERROR)
      frame.texture = 0;
  }
  frame.observation_ms = Milliseconds(start);
  frame.source_gpu_bytes = g_display.reference.bytes + g_display.detail.bytes;
  frame.target_gpu_bytes = static_cast<size_t>(frame.width) * frame.height * 4;
  glBindFramebuffer(GL_FRAMEBUFFER, old_fbo);
  glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);
  glUseProgram(old_program);
  glBindVertexArray(old_vao);
  glActiveTexture(old_active);
  if (blend)
    glEnable(GL_BLEND);
  if (scissor)
    glEnable(GL_SCISSOR_TEST);
  return frame;
}

}  // namespace lumice::gui

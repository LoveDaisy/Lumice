#include "gui/analyze_overview.hpp"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>

#include "gui/gui_constants.hpp"
#include "gui/theme.hpp"
#include "imgui.h"

namespace lumice::gui {
namespace {

// The overview's per-section object cap: the expander is a detail view, not the document. Rows
// past the cap are counted, never hidden silently — the "+N more" line names them (R7).
constexpr size_t kSurveyOverviewMaxObjects = 8;

std::string FormatDeg(double value) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.3f°", value);
  return buf;
}

std::string FormatNum(double value) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.2f", value);
  return buf;
}

// The engine's direction tokens, as the user reads them. Anything else passes through verbatim —
// an unknown token is the engine's fact to state, not ours to guess.
std::string DirectionProse(const std::string& direction) {
  if (direction == "red_inner_blue_outer") {
    return "red inner, blue outer";
  }
  if (direction == "blue_inner_red_outer") {
    return "blue inner, red outer";
  }
  if (direction == "blue_tinted") {
    return "blue-tinted";
  }
  if (direction == "red_tinted") {
    return "red-tinted";
  }
  return direction;
}

// The support card's intervals, as one "[a°, b°] + k more" span (the model's conclusions quote
// the first interval; the full list stays the support card's detail).
bool SupportSpanText(const LUMICE_PathSurvey& result, size_t object_index, std::string* out) {
  LUMICE_PathSurveySupport support{};
  support.struct_size = sizeof(support);
  if (LUMICE_PathSurveyGetSupport(&result, object_index, &support) != LUMICE_PATH_READ_OK ||
      support.interval_count == 0) {
    return false;
  }
  LUMICE_PathSurveyInterval interval{};
  interval.struct_size = sizeof(interval);
  if (LUMICE_PathSurveyGetSupportInterval(&result, object_index, 0, &interval) != LUMICE_PATH_READ_OK) {
    return false;
  }
  char buf[96];
  std::snprintf(buf, sizeof(buf), "from %.3f° to %.3f°", interval.lower_deg, interval.upper_deg);
  *out = buf;
  if (support.interval_count > 1) {
    *out += " + " + std::to_string(support.interval_count - 1) + " more interval(s)";
  }
  return true;
}

void ReadDispersion(const LUMICE_PathSurvey& result, const LUMICE_PathChromatic& chromatic,
                    SurveyOverviewModel* model) {
  if (chromatic.dispersion_available == 0) {
    return;
  }
  LUMICE_PathDispersion dispersion{};
  dispersion.struct_size = sizeof(dispersion);
  if (LUMICE_PathSurveyGetDispersion(&result, &dispersion) != LUMICE_PATH_READ_OK) {
    return;
  }
  SurveyDispersionView& view = model->dispersion;
  view.available = true;
  view.red_label = SurveyTextString(dispersion.red_label);
  view.blue_label = SurveyTextString(dispersion.blue_label);
  view.n_red = dispersion.n_red;
  view.n_blue = dispersion.n_blue;
  view.min_row_available = dispersion.min_row_available != 0;
  view.value_red_deg = dispersion.value_red_deg;
  view.value_blue_deg = dispersion.value_blue_deg;
  view.displacement_deg = dispersion.displacement_deg;
  view.direction = SurveyTextString(dispersion.direction);
  for (size_t i = 0; i < dispersion.followed_count; ++i) {
    LUMICE_PathFollowedOnset onset{};
    onset.struct_size = sizeof(onset);
    if (LUMICE_PathSurveyGetFollowedOnset(&result, i, &onset) != LUMICE_PATH_READ_OK || onset.value_count < 2) {
      continue;
    }
    SurveyFollowedOnsetView row;
    row.source = SurveyTextString(onset.source);
    row.profile = SurveyTextString(onset.profile);
    row.value_red_deg = onset.values_deg[0];
    row.value_blue_deg = onset.values_deg[1];
    row.displacement_deg = onset.displacement_deg;
    row.direction = SurveyTextString(onset.direction);
    view.followed.push_back(std::move(row));
  }
  LUMICE_PathSurveyEvidence evidence{};
  evidence.struct_size = sizeof(evidence);
  if (LUMICE_PathSurveyGetEvidence(&result, LUMICE_PATH_EVIDENCE_DISPERSION, 0, &evidence) == LUMICE_PATH_READ_OK) {
    view.evidence_basis = SurveyTextString(evidence.geometry_basis);
  }
}

void ReadDiagnose(const LUMICE_PathSurvey& result, const LUMICE_PathChromatic& chromatic, SurveyOverviewModel* model) {
  if (chromatic.diagnose_available == 0) {
    return;
  }
  LUMICE_PathDiagnose diagnose{};
  diagnose.struct_size = sizeof(diagnose);
  if (LUMICE_PathSurveyGetDiagnose(&result, &diagnose) != LUMICE_PATH_READ_OK) {
    return;
  }
  SurveyDiagnoseView& view = model->diagnose;
  view.available = true;
  view.verdict_kind = SurveyTextString(diagnose.verdict_kind);
  view.color = SurveyTextString(diagnose.color);
  view.visible = diagnose.visible;
  view.position_available = diagnose.position_available != 0;
  view.position_deg = diagnose.position_deg;
  view.coverage_complete = diagnose.coverage_complete != 0;
  for (size_t i = 0; i < diagnose.note_count; ++i) {
    LUMICE_PathText note{};
    if (LUMICE_PathSurveyGetDiagnoseNote(&result, i, &note) == LUMICE_PATH_READ_OK) {
      view.notes.push_back(SurveyTextString(note));
    }
  }
  LUMICE_PathSurveyEvidence evidence{};
  evidence.struct_size = sizeof(evidence);
  if (LUMICE_PathSurveyGetEvidence(&result, LUMICE_PATH_EVIDENCE_DIAGNOSE, 0, &evidence) == LUMICE_PATH_READ_OK) {
    view.evidence_basis = SurveyTextString(evidence.geometry_basis);
  }
}

void ReadProbe(const LUMICE_PathSurvey& result, SurveyOverviewModel* model) {
  if (model->probe_count == 0) {
    return;
  }
  LUMICE_PathSurveyProbe probe{};
  probe.struct_size = sizeof(probe);
  if (LUMICE_PathSurveyGetProbe(&result, 0, &probe) != LUMICE_PATH_READ_OK) {
    return;
  }
  SurveyProbeView& view = model->probe;
  view.available = true;
  view.state = probe.state;
  view.mode = SurveyTextString(probe.reason);
  view.wavelength_nm = probe.wavelength_nm;
  view.refractive_index = probe.refractive_index;
  view.edge_available = probe.edge_available != 0;
  view.edge_deg = probe.edge_deg;
  view.peak_available = probe.peak_available != 0;
  view.peak_deg = probe.peak_deg;
  view.peak_value = probe.peak_value;
  view.separation_available = probe.separation_available != 0;
  view.separation_deg = probe.separation_deg;
  view.kept_count = probe.kept_count;
  if (probe.state == LUMICE_PATH_STATE_REFUSED) {
    return;  // a refused probe names why instead of publishing readings it did not run
  }
  for (size_t i = 0; i < probe.sample_row_count; ++i) {
    LUMICE_PathProbeSample sample{};
    sample.struct_size = sizeof(sample);
    if (LUMICE_PathSurveyGetProbeSample(&result, 0, i, &sample) != LUMICE_PATH_READ_OK) {
      continue;
    }
    view.sample_delta_deg.push_back(sample.delta_deg);
    view.sample_value.push_back(sample.value);  // an empty bin arrives as NaN — a gap, not a zero
  }
}

void ReadTint(const LUMICE_PathSurvey& result, SurveyOverviewModel* model) {
  LUMICE_PathTint tint{};
  tint.struct_size = sizeof(tint);
  if (LUMICE_PathSurveyGetTint(&result, &tint) != LUMICE_PATH_READ_OK) {
    return;
  }
  SurveyTintView& view = model->tint;
  view.available = true;
  view.state = tint.state;
  view.reading_available = tint.reading_available != 0;
  view.ratio = tint.ratio;
  view.direction = SurveyTextString(tint.direction);
  view.caliber = SurveyTextString(tint.caliber);
}

void ReadWitness(const LUMICE_PathSurvey& result, SurveyOverviewModel* model) {
  for (size_t i = 0; i < model->object_count; ++i) {
    LUMICE_PathSurveyObject obj{};
    obj.struct_size = sizeof(obj);
    if (LUMICE_PathSurveyGetObject(&result, i, &obj) != LUMICE_PATH_READ_OK || obj.witness_available == 0) {
      continue;
    }
    LUMICE_PathWitness witness{};
    witness.struct_size = sizeof(witness);
    if (LUMICE_PathSurveyGetWitness(&result, i, &witness) != LUMICE_PATH_READ_OK) {
      continue;
    }
    SurveyWitnessView& view = model->witness;
    view.available = true;
    view.value_available = witness.value_available != 0;
    view.weight = witness.weight;
    view.kept_count = witness.kept_count;
    view.sample_count = witness.sample_count;
    return;
  }
}

void ReadTir(const LUMICE_PathSurvey& result, SurveyOverviewModel* model) {
  LUMICE_PathTirShare tir{};
  tir.struct_size = sizeof(tir);
  if (LUMICE_PathSurveyGetTirShare(&result, &tir) != LUMICE_PATH_READ_OK) {
    return;
  }
  SurveyTirView& view = model->tir;
  view.available = true;
  view.state = tir.state;
  view.value_available = tir.value_available != 0;
  view.share_all_tir_red = tir.share_all_tir_red;
  view.share_all_tir_blue = tir.share_all_tir_blue;
  view.interface_count = tir.interface_count;
  view.mode = SurveyTextString(tir.mode);
}

void ReadFamily(const LUMICE_PathSurvey& result, SurveyOverviewModel* model) {
  LUMICE_PathFamilyInfo family{};
  family.struct_size = sizeof(family);
  if (LUMICE_PathSurveyGetFamily(&result, &family) != LUMICE_PATH_READ_OK) {
    return;
  }
  SurveyFamilyView& view = model->family;
  view.available = true;
  view.state = family.state;
  view.refusal_mode = SurveyTextString(family.level0_refusal_mode);
  for (size_t l = 0; l < family.ladder_count; ++l) {
    LUMICE_PathFamilyLevel level{};
    level.struct_size = sizeof(level);
    if (LUMICE_PathSurveyGetFamilyLevel(&result, l, &level) != LUMICE_PATH_READ_OK) {
      continue;
    }
    SurveyFamilyLevelView row;
    row.level = level.level;
    for (size_t d = 0; d < level.frozen_dim_count; ++d) {
      LUMICE_PathFamilyFrozenDim dim{};
      dim.struct_size = sizeof(dim);
      if (LUMICE_PathSurveyGetFamilyFrozenDim(&result, l, d, &dim) == LUMICE_PATH_READ_OK) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s at %.1f°±%.1f°", SurveyTextString(dim.dim).c_str(), dim.center_deg,
                      dim.width_deg);
        row.frozen_dims.emplace_back(buf);
      }
    }
    for (size_t d = 0; d < level.free_dim_count; ++d) {
      LUMICE_PathText free_dim{};
      if (LUMICE_PathSurveyGetFamilyFreeDim(&result, l, d, &free_dim) == LUMICE_PATH_READ_OK) {
        row.free_dims.push_back(SurveyTextString(free_dim));
      }
    }
    row.samples = level.samples;
    row.valid_samples = level.valid_samples;
    row.objects_proposed = level.objects_proposed;
    row.objects_killed = level.objects_killed;
    view.levels.push_back(std::move(row));
  }
}

// The main conclusions, per arm. Each line is a fact the document already carries — organized
// and worded here, never invented.
void BuildConclusions(const LUMICE_PathSurvey& result, SurveyOverviewModel* model) {
  std::vector<std::string>& lines = model->conclusions;
  switch (model->arm) {
    case SurveyArm::kIdentity: {
      lines.push_back(
          "Source image (point mass): every pose of this class sends its light back to the sun's own direction — "
          "the class's sky object is the sun itself, certified.");
      if (model->tint.available && model->tint.reading_available) {
        lines.push_back("Weight tint: " + DirectionProse(model->tint.direction) + " (blue/red ratio " +
                        FormatNum(model->tint.ratio) + ") — the colour question this class still has.");
      }
      lines.push_back(
          "No positional dispersion: the outgoing direction is pose-invariant, so wavelength moves no edge — the "
          "colour reading is the weight tint.");
      break;
    }
    case SurveyArm::kFamily: {
      // The freezing ladder's mechanism text (which level froze which dim at what width) is
      // the owner's own example of presentation noise at the default layer — the enumeration
      // lives in Physics details, the per-level sample/survival counts in Calculation details.
      // What floats here is the ladder's consequence in one user sentence: how many images
      // the class holds after the survival filter had its say.
      int surviving = 0;
      int killed = 0;
      for (const SurveyFamilyLevelView& level : model->family.levels) {
        surviving += level.objects_proposed - level.objects_killed;
        killed += level.objects_killed;
      }
      lines.push_back(std::to_string(surviving) + " candidate image(s) survive the ladder; " + std::to_string(killed) +
                      " killed by the survival filter.");
      break;
    }
    case SurveyArm::kRefused: {
      lines.push_back("Support refused (" + model->evidence_mode +
                      "): the class's delta-axis support is unknown. This names a geometry the axis machine could "
                      "not partition — it is not a statement that the class carries no light.");
      if (model->witness.available && model->witness.value_available) {
        lines.push_back(
            "An independent witness still reads positive: the declared source's own event on this "
            "class's lattice, untouched by the refused axis.");
      }
      break;
    }
    case SurveyArm::kFullDimensional: {
      for (size_t i = 0; i < model->object_count; ++i) {
        LUMICE_PathSurveyObject obj{};
        obj.struct_size = sizeof(obj);
        if (LUMICE_PathSurveyGetObject(&result, i, &obj) != LUMICE_PATH_READ_OK) {
          continue;
        }
        if (obj.kind == LUMICE_PATH_SURVEY_SUPPORT_SET) {
          std::string span;
          if (SupportSpanText(result, i, &span)) {
            lines.push_back("Support: " + span + ".");
          }
        }
        if (obj.kind == LUMICE_PATH_SURVEY_CAUSTIC_EDGE && obj.state != LUMICE_PATH_STATE_REFUSED) {
          LUMICE_PathSurveyEdge edge{};
          edge.struct_size = sizeof(edge);
          if (LUMICE_PathSurveyGetEdge(&result, i, &edge) == LUMICE_PATH_READ_OK) {
            lines.push_back(std::string("Sharp edge at ") + FormatDeg(edge.position_deg) +
                            (edge.certificate_available ? " — certified by the closed-form minimum." : " (finite)."));
          }
        }
        if (obj.kind == LUMICE_PATH_SURVEY_BRIGHT_DOMAIN) {
          LUMICE_PathSurveyBright bright{};
          bright.struct_size = sizeof(bright);
          if (LUMICE_PathSurveyGetBright(&result, i, &bright) == LUMICE_PATH_READ_OK) {
            if (bright.peak_available != 0) {
              lines.push_back(std::string("Brightness peaks near ") + FormatDeg(bright.peak_delta_deg) +
                              " — a probe reading at finite resolution, distinct from the geometric edge.");
            } else {
              lines.push_back(
                  "Bright region without a sharp peak or edge — a wide, diffuse structure is a normal "
                  "reading, not a failure to find one.");
            }
          }
        }
      }
      if (model->dispersion.available && model->dispersion.min_row_available) {
        lines.push_back("Wavelength dispersion: the " + model->dispersion.red_label + " inner edge sits " +
                        FormatDeg(model->dispersion.displacement_deg) + " inside the " + model->dispersion.blue_label +
                        " one (" + DirectionProse(model->dispersion.direction) + ").");
      }
      break;
    }
  }
}

}  // namespace

std::string SurveyTextString(const LUMICE_PathText& text) {
  if (text.data == nullptr || text.length == 0) {
    return {};
  }
  return std::string(text.data, text.length);
}

const char* SurveyStateName(int state) {
  switch (state) {
    case LUMICE_PATH_STATE_CERTIFIED:
      return "certified";
    case LUMICE_PATH_STATE_FINITE:
      return "finite";
    case LUMICE_PATH_STATE_REFUSED:
      return "refused";
    case LUMICE_PATH_STATE_UNEVALUATED:
      return "unevaluated";
    default:
      return "unknown state";
  }
}

const char* SurveyOutcomeName(int execution) {
  switch (execution) {
    case LUMICE_PATH_OUTCOME_COMPLETED:
      return "completed";
    case LUMICE_PATH_OUTCOME_CANCELLED:
      return "cancelled";
    case LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED:
      return "deadline exceeded";
    default:
      return "unknown outcome";
  }
}

const char* SurveyObjectKindName(int kind) {
  switch (kind) {
    case LUMICE_PATH_SURVEY_SOURCE_IMAGE:
      return "source image";
    case LUMICE_PATH_SURVEY_SUPPORT_SET:
      return "support set";
    case LUMICE_PATH_SURVEY_CAUSTIC_EDGE:
      return "caustic edge";
    case LUMICE_PATH_SURVEY_BRIGHT_DOMAIN:
      return "bright domain";
    case LUMICE_PATH_SURVEY_FAMILY_COLLAPSE:
      return "family collapse";
    case LUMICE_PATH_SURVEY_REGULAR_IMAGE:
      return "regular image";
    default:
      return "object";
  }
}

SurveyOverviewModel BuildSurveyOverviewModel(const LUMICE_PathSurvey& result) {
  SurveyOverviewModel model;
  LUMICE_PathSurveyInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathSurveyGetInfo(&result, &info) != LUMICE_PATH_READ_OK) {
    return model;  // valid stays false — the renderer states the unreadable document
  }
  model.valid = true;
  model.execution = info.execution;
  model.early_stopped = info.early_stopped != 0;
  model.member_count = info.member_count;
  model.object_count = info.object_count;
  model.unevaluated_count = info.unevaluated_count;
  model.probe_count = info.probe_count;
  model.budget_ms = info.budget_ms;
  model.evidence_state = info.evidence_state;

  LUMICE_PathSurveyScope scope{};
  scope.struct_size = sizeof(scope);
  if (LUMICE_PathSurveyGetScope(&result, &scope) == LUMICE_PATH_READ_OK) {
    model.scope_available = true;
    model.wavelength_nm = scope.wavelength_nm;
    model.wavelength_source = SurveyTextString(scope.wavelength_source);
    model.refractive_index = scope.refractive_index;
    model.sun_available = scope.sun_altitude_available != 0;
    model.sun_altitude_deg = scope.sun_altitude_deg;
    model.sun_azimuth_deg = scope.sun_azimuth_deg;
  }

  // The arm, most-specific flag first: a family document also carries support/edge/domain
  // cards, and a refused document carries produced quantities beside the refusal.
  //
  // The early_stopped half of the identity test is the identity fast path's OWN flag, not a
  // generic "partial" signal: the engine sets it only on the all-rank-zero branch (survey
  // assembly), a query cut by its deadline returns before that branch with the flag false, and
  // a cut inside a later leg leaves it false too — the execution outcome carries the deadline.
  // So "early_stopped && tint" cannot read a timed-out full-dimensional document as identity:
  // a deadline document has the flag false whatever its tint leg managed to produce. Pinned
  // from the GUI side by AnalyzeOverviewChain.DeadlineDocumentKeepsItsArmAndItsProducedLegs.
  if (info.family_available != 0) {
    model.arm = SurveyArm::kFamily;
  } else if (info.early_stopped != 0 && info.tint_available != 0) {
    model.arm = SurveyArm::kIdentity;
  } else if (info.evidence_state == LUMICE_PATH_STATE_REFUSED) {
    model.arm = SurveyArm::kRefused;
  } else {
    model.arm = SurveyArm::kFullDimensional;
  }
  {
    LUMICE_PathSurveyEvidence evidence{};
    evidence.struct_size = sizeof(evidence);
    if (LUMICE_PathSurveyGetEvidence(&result, LUMICE_PATH_EVIDENCE_DOCUMENT, 0, &evidence) == LUMICE_PATH_READ_OK) {
      model.evidence_mode = SurveyTextString(evidence.mode);
    }
  }

  ReadTint(result, &model);
  ReadWitness(result, &model);
  ReadTir(result, &model);
  ReadProbe(result, &model);
  ReadFamily(result, &model);
  {
    LUMICE_PathChromatic chromatic{};
    chromatic.struct_size = sizeof(chromatic);
    if (LUMICE_PathSurveyGetChromatic(&result, &chromatic) == LUMICE_PATH_READ_OK) {
      ReadDispersion(result, chromatic, &model);
      ReadDiagnose(result, chromatic, &model);
      model.chromatic_state = chromatic.state;
      model.chromatic_note = SurveyTextString(chromatic.reason);
    }
  }
  for (size_t i = 0; i < model.object_count; ++i) {
    LUMICE_PathSurveyObject obj{};
    obj.struct_size = sizeof(obj);
    if (LUMICE_PathSurveyGetObject(&result, i, &obj) != LUMICE_PATH_READ_OK) {
      continue;
    }
    SurveyObjectRow row;
    row.kind_name = SurveyObjectKindName(obj.kind);
    row.state = obj.state;
    row.state_name = SurveyStateName(obj.state);
    row.position_available = obj.position_available != 0;
    row.position_altitude_deg = obj.position_altitude_deg;
    row.position_azimuth_deg = obj.position_azimuth_deg;
    row.family_level = obj.state == LUMICE_PATH_STATE_REFUSED ? -1 : static_cast<int>(obj.family_level) - 1;
    row.blur_deg = obj.blur_deg;
    row.reason = SurveyTextString(obj.reason);
    if (row.reason.empty()) {
      row.reason = SurveyTextString(obj.mode);
    }
    model.objects.push_back(std::move(row));
  }
  // The caustic-edge certificate, where the dispersion basis can point at it.
  for (size_t i = 0; i < model.object_count; ++i) {
    LUMICE_PathSurveyObject obj{};
    obj.struct_size = sizeof(obj);
    if (LUMICE_PathSurveyGetObject(&result, i, &obj) != LUMICE_PATH_READ_OK ||
        obj.kind != LUMICE_PATH_SURVEY_CAUSTIC_EDGE) {
      continue;
    }
    LUMICE_PathSurveyEdge edge{};
    edge.struct_size = sizeof(edge);
    if (LUMICE_PathSurveyGetEdge(&result, i, &edge) == LUMICE_PATH_READ_OK && edge.certificate_available != 0) {
      model.dispersion.certificate_available = true;
      model.dispersion.certificate_formula = SurveyTextString(edge.formula);
      model.dispersion.certificate_apex_deg = edge.apex_angle_deg;
      model.dispersion.certificate_cross_check_deg = edge.cross_check_deg;
      model.dispersion.certificate_tolerance_deg = edge.tolerance_deg;
      break;
    }
  }

  BuildConclusions(result, &model);
  return model;
}

// ---- The renderer -----------------------------------------------------------------------------------

namespace {

// One labeled degree: the block texts keep three decimals, the unit is part of the number.
void TextDeg(const char* label, double deg) {
  ImGui::TextWrapped("%s %s", label, FormatDeg(deg).c_str());
}

// The region's text wraps, never clips: ImGui's Text/TextDisabled/BulletText size a single line
// and clip at the region's right edge — exactly where a sentence gets interesting at the right
// column's 260 pt floor (the screenshot that caught this reads "...certified by the closed-form
// minimu"). DisabledWrapped is the color pair by hand (no DisabledTextWrapped in ImGui); a
// wrapping bullet is the Bullet()+TextWrapped pair.
void DisabledWrapped(const char* fmt, ...) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
  va_list args;
  va_start(args, fmt);
  ImGui::TextWrappedV(fmt, args);
  va_end(args);
  ImGui::PopStyleColor();
}

// A fold group's header: the display spelling comes from the ONE label constant; the ID suffix
// keeps same-label groups apart inside one window (the Raypath view hosts the survey answer and
// the point answer side by side, each with its own Physics/Calculation pair — identical visible
// labels would collide in the window's ID space and share one expansion state). No
// DefaultOpen: closed by default, expansion in the host window's storage — session-sticky,
// restart-reset (io.IniFilename is null), independent per window.
bool FoldOpen(const char* label, const char* suffix_id) {
  const std::string id = std::string(label) + "##" + suffix_id;
  return ImGui::CollapsingHeader(id.c_str());
}

// The default layer (the disposition ladder's L1/L2): the answers, their limits and their
// premises — everything that must stay readable with the fold groups closed (the coverage law:
// an L1 fact is never demoted into a fold for space). Shared verbatim by the complete answer
// (the Raypath view's class-answer host) and the main window's summary projection (§16.2: the
// summary is a read of the same layer, not a second wording). H1's gap readings — the diagnose
// verdict, the tint and TIR readings the conclusions don't already carry — read the same model
// fields here, so both hosts see them without a second model.
void RenderSurveyDefaultLayer(const SurveyOverviewModel& model) {
  // The limits first (§16.1: state honesty covers both layers; the disabled block the header
  // used to carry keeps its content, minus the counts line that moved into Calculation
  // details).
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
  if (model.execution == LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED || model.early_stopped) {
    ImGui::TextWrapped("The survey stopped inside its budget — the facts on show are partial.");
  }
  if (model.unevaluated_count > 0) {
    ImGui::TextWrapped("%zu object(s) unevaluated.", model.unevaluated_count);
  }
  // The λ/sun premise rides the answer (§16.2: 必要 λ/太阳前提随答案) — one line each, the same
  // read-back the header block carried.
  if (model.scope_available) {
    ImGui::TextWrapped("Reference wavelength %.1f nm (source: %s, n = %.4f).", model.wavelength_nm,
                       model.wavelength_source.c_str(), model.refractive_index);
    if (model.sun_available) {
      ImGui::TextWrapped("Sun at altitude %.1f°, azimuth %.1f°.", model.sun_altitude_deg, model.sun_azimuth_deg);
    }
  }
  ImGui::PopStyleColor();
  // The main conclusions wrap: they are the region's primary content, and a 400px panel must
  // show every word of them — BulletText would clip at the panel edge exactly where a sentence
  // gets interesting (the screenshot that caught this reads "...certified by the closed-form
  // minimu").
  for (const std::string& line : model.conclusions) {
    ImGui::Bullet();
    ImGui::TextWrapped("%s", line.c_str());
  }
  // The chromatic block that ran neither half states so in the engine's own words — an
  // unevaluated leg is a fact on show, never a silent absence (R7).
  if (!model.dispersion.available && !model.diagnose.available && !model.chromatic_note.empty()) {
    DisabledWrapped("Chromatic layer (%s): %s", SurveyStateName(model.chromatic_state), model.chromatic_note.c_str());
  }
  // The colour verdict's main conclusion, default (the Dispersion/Diagnose row: 边位置、可见性/
  // 色彩主结论默认；coverage 不完整默认). Its notes and evidence basis stay in Physics details.
  if (model.diagnose.available) {
    ImGui::Bullet();
    ImGui::TextWrapped("Weight colour: %s, %s, %s.", model.diagnose.verdict_kind.c_str(), model.diagnose.color.c_str(),
                       model.diagnose.visible ? "visible" : "not visible");
    if (model.diagnose.position_available) {
      TextDeg("Position:", model.diagnose.position_deg);
    }
    if (!model.diagnose.coverage_complete) {
      DisabledWrapped("Coverage incomplete — the verdict names what it read, not the whole class.");
    }
  }
  // The tint reading for the arms whose conclusions don't already carry it (the identity arm's
  // conclusions quote the ratio; the caliber stays a Physics detail).
  if (model.tint.available && model.tint.reading_available && model.arm != SurveyArm::kIdentity) {
    ImGui::Bullet();
    ImGui::TextWrapped("Blue/red ratio %.3f — %s.", model.tint.ratio, DirectionProse(model.tint.direction).c_str());
  }
  // The probe's readings for the arms whose conclusions don't carry the edge/peak facts (the
  // full-dimensional arm's object cards name both); a refused probe's named mode is a limit.
  if (model.probe.available && model.arm != SurveyArm::kFullDimensional) {
    if (model.probe.state == LUMICE_PATH_STATE_REFUSED) {
      DisabledWrapped("Probe refused (%s) — no readings claimed.", model.probe.mode.c_str());
    } else {
      if (model.probe.edge_available) {
        ImGui::Bullet();
        ImGui::TextWrapped("Profile edge %s", FormatDeg(model.probe.edge_deg).c_str());
      }
      if (model.probe.peak_available) {
        ImGui::Bullet();
        ImGui::TextWrapped("Profile peak %s", FormatDeg(model.probe.peak_deg).c_str());
      }
      if (model.probe.separation_available) {
        ImGui::Bullet();
        ImGui::TextWrapped("Peak–edge separation %s", FormatDeg(model.probe.separation_deg).c_str());
      }
    }
  }
  // The witness's quantitative reading on the arms whose conclusions don't carry the witness
  // sentence (the refused arm's do — with the ≠-support qualification verbatim).
  if (model.witness.available && model.witness.value_available && model.arm != SurveyArm::kRefused) {
    ImGui::Bullet();
    ImGui::TextWrapped("Independent witness reads positive: max kept weight %.4g.", model.witness.weight);
  }
  // The family ladder's level-0 refusal is a limit — default (the TIR/Family/Objects row:
  // 拒答/未评估限制默认).
  if (model.family.available && !model.family.refusal_mode.empty()) {
    DisabledWrapped("Freezing ladder refused at level 0 (%s).", model.family.refusal_mode.c_str());
  }
  // The TIR share: the document-level reading and its refusal state, default; the interface
  // count is a Physics detail. Zero-shares read as the finite values they are.
  if (model.tir.available) {
    if (model.tir.value_available) {
      ImGui::Bullet();
      ImGui::TextWrapped("All-TIR share %.3f (red) / %.3f (blue).", model.tir.share_all_tir_red,
                         model.tir.share_all_tir_blue);
    } else if (model.tir.mode.empty()) {
      DisabledWrapped("Internal-reflection share %s — no value and no named mode.", SurveyStateName(model.tir.state));
    } else {
      DisabledWrapped("Internal-reflection share %s (%s).", SurveyStateName(model.tir.state), model.tir.mode.c_str());
    }
  }
}

// The Physics details group (the ladder's L3): the optical premises, evidence methods and
// complete definitions behind the default layer's answers. One closed-by-default fold; each
// former block keeps a small section label so the group stays navigable.
void RenderSurveyPhysicsDetails(const SurveyOverviewModel& model) {
  if (model.dispersion.available) {
    ImGui::TextDisabled("Chromatic dispersion");
    const SurveyDispersionView& d = model.dispersion;
    if (!d.min_row_available) {
      DisabledWrapped("No minimum-deviation fold in this class — the followed onsets are the positional facts.");
    } else {
      ImGui::TextWrapped("%s inner edge %s — %s %s", d.red_label.c_str(), FormatDeg(d.value_red_deg).c_str(),
                         d.blue_label.c_str(), FormatDeg(d.value_blue_deg).c_str());
      DisabledWrapped("Refractive indices n(%s) = %.4f, n(%s) = %.4f — the engine's dispersion table at this pair.",
                      d.red_label.c_str(), d.n_red, d.blue_label.c_str(), d.n_blue);
    }
    if (!d.followed.empty()) {
      if (ImGui::TreeNode("##ov_disp_followed", "Followed onsets (%zu)", d.followed.size())) {
        for (const SurveyFollowedOnsetView& row : d.followed) {
          // Keep these multi-part diagnostics in readable ASCII prose. The body font loads
          // the observed map delta/lambda, not arbitrary mathematical relations or arrows;
          // the analysis list's arrow is a separately merged icon.
          ImGui::Bullet();
          ImGui::TextWrapped("%s/%s: %s to %s (shift %s, %s)", row.source.c_str(), row.profile.c_str(),
                             FormatDeg(row.value_red_deg).c_str(), FormatDeg(row.value_blue_deg).c_str(),
                             FormatDeg(row.displacement_deg).c_str(), DirectionProse(row.direction).c_str());
        }
        ImGui::TreePop();
      }
    }
    if (!d.evidence_basis.empty()) {
      DisabledWrapped("Basis: %s", d.evidence_basis.c_str());
    }
    if (d.certificate_available) {
      DisabledWrapped("Edge certificate: %s with A = %.3f°.", d.certificate_formula.c_str(), d.certificate_apex_deg);
    }
  }
  if (model.diagnose.available) {
    ImGui::TextDisabled("Weight colour reading");
    for (const std::string& note : model.diagnose.notes) {
      DisabledWrapped("%s", note.c_str());
    }
    if (!model.diagnose.evidence_basis.empty()) {
      DisabledWrapped("Basis: %s", model.diagnose.evidence_basis.c_str());
    }
  }
  // The reference profile's curve — the class's brightness along the solar radial line. The
  // readings themselves ride the default layer (or the full-dimensional conclusions); empty
  // samples stay gaps.
  if (model.probe.available && model.probe.state != LUMICE_PATH_STATE_REFUSED) {
    const SurveyProbeView& p = model.probe;
    ImGui::TextDisabled("Reference profile — %.0f nm", p.wavelength_nm);
    if (p.sample_delta_deg.size() >= 2) {
      double max_value = 0.0;
      for (const double v : p.sample_value) {
        if (!std::isnan(v) && v > max_value) {
          max_value = v;
        }
      }
      if (max_value > 0.0) {
        const float w = ImGui::GetContentRegionAvail().x;
        const float h = UiPx(64.0f);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(w, h));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const double d0 = p.sample_delta_deg.front();
        const double d1 = p.sample_delta_deg.back();
        const float plot_w = w - 2.0f;
        auto to_screen = [&](double delta, double value) {
          const float x = origin.x + 1.0f + static_cast<float>((delta - d0) / (d1 - d0)) * plot_w;
          const float y = origin.y + h - static_cast<float>(value / max_value) * (h - 2.0f) - 1.0f;
          return ImVec2(x, y);
        };
        bool pen_down = false;
        ImVec2 prev(0.0f, 0.0f);
        const ImU32 line_col = ImGui::GetColorU32(ImGuiCol_PlotLines);
        for (size_t i = 0; i < p.sample_value.size(); ++i) {
          const double v = p.sample_value[i];
          if (std::isnan(v)) {  // an empty bin: lift the pen, never draw a zero
            pen_down = false;
            continue;
          }
          const ImVec2 pt = to_screen(p.sample_delta_deg[i], v);
          if (pen_down) {
            dl->AddLine(prev, pt, line_col, 1.0f);
          }
          prev = pt;
          pen_down = true;
        }
        // The readings' positions on the same ruler: the edge dim, the peak accented.
        if (p.edge_available && p.edge_deg >= d0 && p.edge_deg <= d1) {
          const ImVec2 a = to_screen(p.edge_deg, 0.0);
          const ImVec2 b = to_screen(p.edge_deg, max_value);
          dl->AddLine(a, b, ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
        }
        if (p.peak_available && p.peak_deg >= d0 && p.peak_deg <= d1) {
          const ImVec2 a = to_screen(p.peak_deg, 0.0);
          const ImVec2 b = to_screen(p.peak_deg, max_value);
          dl->AddLine(a, b, ImGui::GetColorU32(ImGuiCol_PlotHistogram), 1.0f);
        }
        DisabledWrapped("Deviation from %s to %s — empty bins are gaps, not zeros; n = %.4f at this wavelength.",
                        FormatDeg(d0).c_str(), FormatDeg(d1).c_str(), p.refractive_index);
      }
    }
  }
  if (model.tint.available && !model.tint.caliber.empty()) {
    ImGui::TextDisabled("Weight tint");
    DisabledWrapped("Caliber: %s", model.tint.caliber.c_str());
  }
  if (model.witness.available) {
    ImGui::TextDisabled("Independent witness");
    if (model.witness.value_available) {
      ImGui::TextWrapped("Max kept weight %.4g.", model.witness.weight);
      DisabledWrapped("Read on the declared source's own lattice — independent of the refused delta axis.");
    } else {
      DisabledWrapped("No readable witness value.");
    }
  }
  if (model.tir.available && model.tir.value_available) {
    ImGui::TextDisabled("Internal-reflection share");
    ImGui::TextWrapped("Read across %zu interface(s).", model.tir.interface_count);
  }
  if (model.family.available) {
    ImGui::TextDisabled("Freezing ladder");
    for (const SurveyFamilyLevelView& level : model.family.levels) {
      if (!level.frozen_dims.empty()) {
        std::string frozen = "L" + std::to_string(level.level) + " frozen: ";
        for (const std::string& dim : level.frozen_dims) {
          frozen += dim + "; ";
        }
        DisabledWrapped("%s", frozen.c_str());
      }
      if (!level.free_dims.empty()) {
        std::string free_dims = "L" + std::to_string(level.level) + " free: ";
        for (const std::string& dim : level.free_dims) {
          free_dims += dim + "; ";
        }
        DisabledWrapped("%s", free_dims.c_str());
      }
    }
  }
  if (!model.objects.empty()) {
    ImGui::TextDisabled("Objects");
    const size_t shown = std::min<size_t>(model.objects.size(), kSurveyOverviewMaxObjects);
    for (size_t i = 0; i < shown; ++i) {
      const SurveyObjectRow& row = model.objects[i];
      std::string label = row.kind_name;
      if (row.family_level >= 0) {
        label = "L" + std::to_string(row.family_level) + " " + label;
      }
      label += std::string(" — ") + row.state_name;
      if (row.position_available) {
        char pos[96];
        std::snprintf(pos, sizeof(pos), " at %.2f°/%.2f°", row.position_altitude_deg, row.position_azimuth_deg);
        label += pos;
      }
      if (row.blur_deg > 0.0) {
        char blur[48];
        std::snprintf(blur, sizeof(blur), ", blur %s", FormatDeg(row.blur_deg).c_str());
        label += blur;
      }
      ImGui::Bullet();
      ImGui::TextWrapped("%s", label.c_str());
      if (!row.reason.empty()) {
        DisabledWrapped("%s", row.reason.c_str());
      }
    }
    // The cap's truncation note stays AT the truncated list (the ladder's L2: the +N hint
    // lives where the cut happened).
    if (model.objects.size() > shown) {
      DisabledWrapped("+ %zu more object(s) — beyond this view.", model.objects.size() - shown);
    }
  }
}

// The Calculation details group (the ladder's L4): the numeric and execution facts a check
// reads — counts, budgets, tolerances — none of them a default answer.
void RenderSurveyCalculationDetails(const SurveyOverviewModel& model) {
  ImGui::TextWrapped("%s — %zu member(s), %zu object(s), budget %d ms%s", SurveyOutcomeName(model.execution),
                     model.member_count, model.object_count, model.budget_ms,
                     model.early_stopped ? ", early-stopped" : "");
  if (model.dispersion.certificate_available) {
    DisabledWrapped("Edge certificate cross-check %.1e° against tolerance %.1e°.",
                    model.dispersion.certificate_cross_check_deg, model.dispersion.certificate_tolerance_deg);
  }
  if (model.probe.available) {
    ImGui::TextWrapped("Probe: %lld kept lattice events.", static_cast<long long>(model.probe.kept_count));
  }
  if (model.witness.available && model.witness.value_available) {
    ImGui::TextWrapped("Witness lattice: %lld positive of %lld samples.",
                       static_cast<long long>(model.witness.kept_count),
                       static_cast<long long>(model.witness.sample_count));
  }
  if (model.family.available) {
    for (const SurveyFamilyLevelView& level : model.family.levels) {
      ImGui::Bullet();
      ImGui::TextWrapped("L%d — %d sample(s), %d valid, %d proposed, %d killed by the survival filter.", level.level,
                         level.samples, level.valid_samples, level.objects_proposed, level.objects_killed);
    }
  }
}

}  // namespace

void RenderSurveyOverviewModel(const SurveyOverviewModel& model) {
  if (!model.valid) {
    ImGui::TextUnformatted("Survey document unreadable.");
    return;
  }
  RenderSurveyDefaultLayer(model);
  ImGui::Spacing();
  if (FoldOpen(kSurveyPhysicsDetailsLabel, "sv_answer")) {
    RenderSurveyPhysicsDetails(model);
    ImGui::Spacing();
  }
  if (FoldOpen(kSurveyCalculationDetailsLabel, "sv_answer")) {
    RenderSurveyCalculationDetails(model);
  }
}

void RenderSurveySummary(const SurveyOverviewModel& model) {
  if (!model.valid) {
    ImGui::TextUnformatted("Survey document unreadable.");
    return;
  }
  RenderSurveyDefaultLayer(model);
  // §16.2: once physical entries are omitted, the pointer must say where the complete answer
  // lives — never a silent trim read as the whole report.
  DisabledWrapped("The complete answer is in the %s.", kRaypathViewWindowTitle);
}

}  // namespace lumice::gui

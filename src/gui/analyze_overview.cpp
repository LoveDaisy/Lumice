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
      if (!model->family.levels.empty()) {
        std::string ladder = "Freezing ladder:";
        for (const SurveyFamilyLevelView& level : model->family.levels) {
          if (level.frozen_dims.empty()) {
            continue;
          }
          ladder += " L" + std::to_string(level.level) + " freezes " + level.frozen_dims.front() + ";";
        }
        lines.push_back(ladder);
      }
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
  ImGui::Text("%s %s", label, FormatDeg(deg).c_str());
}

void DisabledWrapped(const char* fmt, ...) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
  va_list args;
  va_start(args, fmt);
  ImGui::TextWrappedV(fmt, args);
  va_end(args);
  ImGui::PopStyleColor();
}

// The chromatic dispersion block: the min-deviation row with both lambdas named, the followed
// onsets behind an expander, and the basis — the engine's own geometry string plus the certified
// edge's closed-form certificate — readable in place (AC2's 依据可查).
void DispersionBlock(const SurveyDispersionView& d) {
  if (!ImGui::CollapsingHeader("Chromatic dispersion##ov_disp", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  if (!d.min_row_available) {
    DisabledWrapped("No minimum-deviation fold in this class — the followed onsets are the positional facts.");
  } else {
    ImGui::Text("%s inner edge %s — %s %s", d.red_label.c_str(), FormatDeg(d.value_red_deg).c_str(),
                d.blue_label.c_str(), FormatDeg(d.value_blue_deg).c_str());
    ImGui::Text("Displacement %s (%s).", FormatDeg(d.displacement_deg).c_str(), DirectionProse(d.direction).c_str());
    DisabledWrapped("Refractive indices n(%s) = %.4f, n(%s) = %.4f — the engine's dispersion table at this pair.",
                    d.red_label.c_str(), d.n_red, d.blue_label.c_str(), d.n_blue);
  }
  if (!d.followed.empty()) {
    if (ImGui::TreeNode("##ov_disp_followed", "Followed onsets (%zu)", d.followed.size())) {
      for (const SurveyFollowedOnsetView& row : d.followed) {
        // ASCII spellings throughout: the body font's glyph ranges carry Latin plus the
        // punctuation block only (theme.cpp), so Greek deltas, math relations and arrows would
        // rasterize to "?" — the same fact that gave the analysis list its icon joiner.
        ImGui::BulletText("%s/%s: %s to %s (shift %s, %s)", row.source.c_str(), row.profile.c_str(),
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
    DisabledWrapped("Edge certificate: %s with A = %.3f° (cross-check %.1e° against tolerance %.1e°).",
                    d.certificate_formula.c_str(), d.certificate_apex_deg, d.certificate_cross_check_deg,
                    d.certificate_tolerance_deg);
  }
}

// The diagnose block: the weight-side colour reading, kept separate from the positional
// dispersion above (different kernel paths, different mechanisms — AC3's separation).
void DiagnoseBlock(const SurveyDiagnoseView& dg) {
  char header[96];
  std::snprintf(header, sizeof(header), "Weight colour reading (%s)##ov_diag", dg.verdict_kind.c_str());
  if (!ImGui::CollapsingHeader(header)) {
    return;
  }
  ImGui::Text("Verdict: %s, %s, %s.", dg.verdict_kind.c_str(), dg.color.c_str(),
              dg.visible ? "visible" : "not visible");
  if (dg.position_available) {
    TextDeg("Position:", dg.position_deg);
  }
  if (!dg.coverage_complete) {
    DisabledWrapped("Coverage incomplete — the verdict names what it read, not the whole class.");
  }
  for (const std::string& note : dg.notes) {
    DisabledWrapped("%s", note.c_str());
  }
  if (!dg.evidence_basis.empty()) {
    DisabledWrapped("Basis: %s", dg.evidence_basis.c_str());
  }
}

// The reference profile: the class's brightness along the solar radial line at the document's
// reference wavelength. Readings first, the curve behind them; empty samples stay gaps.
void ProbeBlock(const SurveyProbeView& p) {
  char header[96];
  std::snprintf(header, sizeof(header), "Reference profile — %.0f nm##ov_prof", p.wavelength_nm);
  if (!ImGui::CollapsingHeader(header)) {
    return;
  }
  if (p.state == LUMICE_PATH_STATE_REFUSED) {
    DisabledWrapped("Probe refused (%s) — no readings claimed.", p.mode.c_str());
    return;
  }
  if (p.edge_available) {
    ImGui::Text("Edge %s", FormatDeg(p.edge_deg).c_str());
  }
  if (p.peak_available) {
    ImGui::Text("Peak %s", FormatDeg(p.peak_deg).c_str());
  }
  if (p.separation_available) {
    ImGui::Text("Peak–edge separation %s", FormatDeg(p.separation_deg).c_str());
  }
  ImGui::Text("%lld kept lattice events.", static_cast<long long>(p.kept_count));
  if (p.sample_delta_deg.size() < 2) {
    return;
  }
  double max_value = 0.0;
  for (const double v : p.sample_value) {
    if (!std::isnan(v) && v > max_value) {
      max_value = v;
    }
  }
  if (max_value <= 0.0) {
    return;
  }
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

void TintBlock(const SurveyTintView& tint, SurveyArm arm) {
  if (!ImGui::CollapsingHeader("Weight tint##ov_tint",
                               arm == SurveyArm::kIdentity ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
    return;
  }
  if (!tint.reading_available) {
    DisabledWrapped("No reading (%s).", SurveyStateName(tint.state));
    return;
  }
  ImGui::Text("Blue/red ratio %.3f — %s.", tint.ratio, DirectionProse(tint.direction).c_str());
  if (!tint.caliber.empty()) {
    DisabledWrapped("Caliber: %s", tint.caliber.c_str());
  }
}

void WitnessBlock(const SurveyWitnessView& w) {
  if (!ImGui::CollapsingHeader("Independent witness##ov_wit", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  if (!w.value_available) {
    DisabledWrapped("No readable witness value.");
    return;
  }
  ImGui::Text("Positive events kept: %lld of %lld lattice samples (max kept weight %.4g).",
              static_cast<long long>(w.kept_count), static_cast<long long>(w.sample_count), w.weight);
  DisabledWrapped("Read on the declared source's own lattice — independent of the refused delta axis.");
}

void TirBlock(const SurveyTirView& tir) {
  if (!ImGui::CollapsingHeader("Internal-reflection share##ov_tir")) {
    return;
  }
  if (!tir.value_available) {
    if (tir.mode.empty()) {
      DisabledWrapped("Share %s — no value and no named mode.", SurveyStateName(tir.state));
    } else {
      DisabledWrapped("Share %s (%s).", SurveyStateName(tir.state), tir.mode.c_str());
    }
    return;
  }
  ImGui::Text("All-TIR share %.3f (red) / %.3f (blue) across %zu interface(s).", tir.share_all_tir_red,
              tir.share_all_tir_blue, tir.interface_count);
}

void FamilyBlock(const SurveyFamilyView& family) {
  if (!ImGui::CollapsingHeader("Freezing ladder##ov_family", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  if (!family.refusal_mode.empty()) {
    DisabledWrapped("Ladder refused at level 0 (%s).", family.refusal_mode.c_str());
  }
  for (const SurveyFamilyLevelView& level : family.levels) {
    ImGui::BulletText("L%d — %d sample(s), %d valid, %d proposed, %d killed by the survival filter.", level.level,
                      level.samples, level.valid_samples, level.objects_proposed, level.objects_killed);
    if (!level.frozen_dims.empty()) {
      std::string frozen = "frozen: ";
      for (const std::string& dim : level.frozen_dims) {
        frozen += dim + "; ";
      }
      DisabledWrapped("%s", frozen.c_str());
    }
    if (!level.free_dims.empty()) {
      std::string free_dims = "free: ";
      for (const std::string& dim : level.free_dims) {
        free_dims += dim + "; ";
      }
      DisabledWrapped("%s", free_dims.c_str());
    }
  }
}

void ObjectsBlock(const SurveyOverviewModel& model) {
  char header[96];
  std::snprintf(header, sizeof(header), "Objects (%zu)##ov_obj", model.objects.size());
  if (!ImGui::CollapsingHeader(header)) {
    return;
  }
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
    ImGui::BulletText("%s", label.c_str());
    if (!row.reason.empty()) {
      DisabledWrapped("%s", row.reason.c_str());
    }
  }
  if (model.objects.size() > shown) {
    DisabledWrapped("+ %zu more object(s) — beyond this view.", model.objects.size() - shown);
  }
}

}  // namespace

void RenderSurveyOverviewModel(const SurveyOverviewModel& model) {
  if (!model.valid) {
    ImGui::TextUnformatted("Survey document unreadable.");
    return;
  }
  ImGui::BulletText("%s — %zu member(s), %zu object(s), budget %d ms%s", SurveyOutcomeName(model.execution),
                    model.member_count, model.object_count, model.budget_ms,
                    model.early_stopped ? ", early-stopped" : "");
  {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    if (model.execution == LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED || model.early_stopped) {
      ImGui::TextWrapped("The survey stopped inside its budget — the facts on show are partial.");
    }
    if (model.unevaluated_count > 0) {
      ImGui::TextWrapped("%zu object(s) unevaluated.", model.unevaluated_count);
    }
    if (model.scope_available) {
      ImGui::TextWrapped("Reference wavelength %.1f nm (source: %s, n = %.4f).", model.wavelength_nm,
                         model.wavelength_source.c_str(), model.refractive_index);
      if (model.sun_available) {
        ImGui::TextWrapped("Sun at altitude %.1f°, azimuth %.1f°.", model.sun_altitude_deg, model.sun_azimuth_deg);
      }
    }
    ImGui::PopStyleColor();
  }
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

  if (model.dispersion.available) {
    ImGui::Spacing();
    DispersionBlock(model.dispersion);
  }
  if (model.diagnose.available) {
    ImGui::Spacing();
    DiagnoseBlock(model.diagnose);
  }
  if (model.tint.available) {
    ImGui::Spacing();
    TintBlock(model.tint, model.arm);
  }
  if (model.probe.available) {
    ImGui::Spacing();
    ProbeBlock(model.probe);
  }
  if (model.witness.available) {
    ImGui::Spacing();
    WitnessBlock(model.witness);
  }
  if (model.tir.available) {
    ImGui::Spacing();
    TirBlock(model.tir);
  }
  if (model.family.available) {
    ImGui::Spacing();
    FamilyBlock(model.family);
  }
  if (!model.objects.empty()) {
    ImGui::Spacing();
    ObjectsBlock(model);
  }
}

}  // namespace lumice::gui

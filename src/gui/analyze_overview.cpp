#include "gui/analyze_overview.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "gui/analyze_ui_widgets.hpp"  // DisabledWrappedText — the wrapped disabled-colour pair
#include "gui/gui_constants.hpp"
#include "gui/theme.hpp"
#include "imgui.h"
#include "util/env_knobs.hpp"  // env::AnalyzeDebugDetails — the diagnostics groups' env gate

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

// The two-decimal degree form the object's bin readings and chromatic pair are stated in (the
// finite-resolution readings' own precision — three decimals would overstate a 0.1° bin).
std::string FormatDeg2(double value) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.2f°", value);
  return buf;
}

// The corridor's written faces, "3-5" — the sequence the request spelled, the representative the
// probe and the chromatic legs evaluated.
std::string FaceSequenceLabel(const int* faces, size_t count) {
  std::string out;
  for (size_t i = 0; i < count; ++i) {
    if (i > 0) {
      out += '-';
    }
    out += std::to_string(faces[i]);
  }
  return out;
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
  view.azimuth_available = probe.azimuth_available != 0;
  view.azimuth_deg = probe.azimuth_deg;
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

// The C01 first-class inner-edge object (817.2): the qualified corridor's four readings, read
// back from the document the conclusions also read. The qualification gate is the engine's own
// routing verdict plus the cards' own declarations (the header's contract): the full-dimensional
// arm — the only route the engine enters with the uniform random pose family, so the rotation
// invariance the whole-sky wording rests on is the engine's, not a GUI inference — and a
// concluded edge card declaring coincidence with the support set. Everything else leaves the
// view unqualified and the display at its existing lines.
void BuildInnerEdgeView(const LUMICE_PathSurvey& result, SurveyOverviewModel* model, bool point_sun_premise) {
  SurveyInnerEdgeView& view = model->inner_edge;
  // The point-sun premise is the third gate leg, beside the arm and the card: the certificate
  // is the single-direction minimum-deviation identity — with an extended sun the physical
  // light begins before the certified bound (a 2° disc starts ~2° inside it), so "light begins
  // outside this circle" would be false exactly where it reads as a fact. An unproven premise
  // keeps the existing angular-distance presentation.
  if (model->arm != SurveyArm::kFullDimensional || !point_sun_premise) {
    return;
  }
  // The qualified edge card: the first card that is CONCLUDED, declares coincidence with the
  // support set, and carries the closed-form certificate. The certificate is the lower-end
  // proof, not decoration: the producer certifies only an interior-minimum fold (the closed form
  // is the minimum-deviation identity), and the support's lower bound IS that minimum — so a
  // certified coincident card is the support's inner bound, and "light begins outside" follows.
  // A merely finite coincident card may be the support's UPPER bound (the producer's
  // coincidence flag covers both ends; the max-fold corpus document delivers exactly such an
  // upper card at ~149.085° with no certificate), where the outside of the circle is the dark
  // side — upgrading it would invert the physics, so it keeps its existing flat presentation.
  // The gate is the producers' own words end to end; no GUI-side tolerance or direction
  // re-derivation.
  for (size_t i = 0; i < model->object_count; ++i) {
    LUMICE_PathSurveyObject obj{};
    obj.struct_size = sizeof(obj);
    if (LUMICE_PathSurveyGetObject(&result, i, &obj) != LUMICE_PATH_READ_OK ||
        obj.kind != LUMICE_PATH_SURVEY_CAUSTIC_EDGE ||
        (obj.state != LUMICE_PATH_STATE_CERTIFIED && obj.state != LUMICE_PATH_STATE_FINITE)) {
      continue;
    }
    LUMICE_PathSurveyEdge card{};
    card.struct_size = sizeof(card);
    if (LUMICE_PathSurveyGetEdge(&result, i, &card) != LUMICE_PATH_READ_OK || card.coincident_with_support == 0 ||
        card.certificate_available == 0) {
      continue;
    }
    view.qualified = true;
    view.edge_object_index = static_cast<int>(i);
    view.edge_deg = card.position_deg;
    view.certificate_available = card.certificate_available != 0;
    view.measure_limit_available = card.measure_limit_available != 0;
    view.measure_limit = card.measure_limit;
    break;
  }
  if (!view.qualified) {
    return;
  }
  view.wavelength_nm = model->wavelength_nm;
  // The whole-sky premise: the rotation invariance is the engine's routing verdict (the
  // full-dimensional arm's own), and the fixed λ is the survey's own scope — the note states
  // both, so the main sentence's whole-circle claim never reads as a map-λ reading (AC3's
  // fixed-reference branch).
  {
    char note[128];
    std::snprintf(note, sizeof(note),
                  "Whole-sky circle of this random-orientation class, at the survey's fixed %.0f nm.",
                  view.wavelength_nm);
    view.premise_note = note;
  }
  // The member scope's kind (the engine's own vocabulary): the physical class or the literal
  // written sequence the request asked for.
  LUMICE_PathSurveyScope scope{};
  scope.struct_size = sizeof(scope);
  if (LUMICE_PathSurveyGetScope(&result, &scope) == LUMICE_PATH_READ_OK) {
    view.member_kind_available = true;
    view.member_kind = scope.member_kind;
  }
  // The profile readings: the built-in solar radial probe. The bright card's own association
  // names WHICH probe is the corridor's (the engine backfills only the member-matched card);
  // without it the arm's single built-in probe is the read. A refused probe names why — its
  // readings are not claimed.
  size_t probe_index = 0;
  bool probe_known = model->probe_count > 0;
  for (size_t i = 0; i < model->object_count && probe_known; ++i) {
    LUMICE_PathSurveyObject obj{};
    obj.struct_size = sizeof(obj);
    if (LUMICE_PathSurveyGetObject(&result, i, &obj) != LUMICE_PATH_READ_OK ||
        obj.kind != LUMICE_PATH_SURVEY_BRIGHT_DOMAIN) {
      continue;
    }
    LUMICE_PathSurveyBright bright{};
    bright.struct_size = sizeof(bright);
    if (LUMICE_PathSurveyGetBright(&result, i, &bright) != LUMICE_PATH_READ_OK || bright.probe_index_available == 0) {
      continue;
    }
    if (bright.probe_index < model->probe_count) {
      probe_index = bright.probe_index;
    }
    break;
  }
  if (probe_known) {
    LUMICE_PathSurveyProbe probe{};
    probe.struct_size = sizeof(probe);
    if (LUMICE_PathSurveyGetProbe(&result, probe_index, &probe) != LUMICE_PATH_READ_OK) {
      probe_known = false;
    } else {
      view.bin_deg = probe.bin_deg;
      view.wavelength_nm = probe.wavelength_nm;  // the evaluated λ — the premise the readings quote
      if (probe.representative != nullptr && probe.representative_count > 0) {
        view.name = FaceSequenceLabel(probe.representative, probe.representative_count);
      }
      if (probe.state == LUMICE_PATH_STATE_REFUSED) {
        const std::string why =
            "probe refused (" +
            (probe.reason.length > 0 ? SurveyTextString(probe.reason) : std::string("unspecified")) + ")";
        view.first_bin.unavailable = why;
        view.highest_bin.unavailable = why;
      } else {
        if (probe.edge_available != 0) {
          view.first_bin.available = true;
          view.first_bin.deg = probe.edge_deg;
          view.first_bin.caliber = "first occupied bin centre";
        } else {
          view.first_bin.unavailable = "no occupied bin in this cut";
        }
        if (probe.peak_available != 0) {
          view.highest_bin.available = true;
          view.highest_bin.deg = probe.peak_deg;
          view.highest_bin.caliber = "highest bin centre on this cut";
        } else {
          view.highest_bin.unavailable = "no occupied bin in this cut";
        }
      }
    }
  }
  if (view.name.empty()) {
    // No probe named its member: the request's own written faces are the corridor's name.
    LUMICE_PathFaceSequence layer{};
    layer.struct_size = sizeof(layer);
    if (LUMICE_PathSurveyGetRequestLayer(&result, 0, &layer) == LUMICE_PATH_READ_OK && layer.faces != nullptr &&
        layer.face_count > 0) {
      view.name = FaceSequenceLabel(layer.faces, layer.face_count);
    }
  }
  // The fixed chromatic pair: the engine's minimum-deviation row, verbatim.
  if (model->dispersion.available && model->dispersion.min_row_available) {
    view.dispersion_available = true;
    view.dispersion_red_deg = model->dispersion.value_red_deg;
    view.dispersion_blue_deg = model->dispersion.value_blue_deg;
    view.dispersion_gap_deg = model->dispersion.displacement_deg;
    view.dispersion_red_label = model->dispersion.red_label;
    view.dispersion_blue_label = model->dispersion.blue_label;
    view.dispersion_direction = model->dispersion.direction;
  } else if (model->dispersion.available) {
    view.dispersion_unavailable = "no minimum-deviation row in this class";
  } else {
    view.dispersion_unavailable = "the chromatic dispersion leg did not produce a row";
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
      // lives in the env-gated diagnostics group, the per-level sample/survival counts beside
      // it. What floats here is the ladder's consequence in one user sentence: how many images
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
                      "): the class's deviation-axis support is unknown. This names a geometry the axis machine could "
                      "not partition — it is not a statement that the class carries no light.");
      if (model->witness.available && model->witness.value_available) {
        lines.push_back(
            "An independent witness still reads positive: the declared source's own event on this "
            "class's lattice, untouched by the refused axis.");
      }
      break;
    }
    case SurveyArm::kFullDimensional: {
      const SurveyInnerEdgeView& edge_view = model->inner_edge;
      // The qualified object's main sentence leads (AC2: the class answer's first read is the
      // inner-edge object, before any other structure); its four readings render in the
      // interactive host's expander, so the flat lines they came from do not repeat here.
      if (edge_view.qualified) {
        char sentence[192];
        std::snprintf(sentence, sizeof(sentence),
                      "Inner edge (%s): about %.2f° from the sun — outside this circle the path's light begins.",
                      edge_view.name.c_str(), edge_view.edge_deg);
        lines.push_back(sentence);
      }
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
          if (edge_view.qualified && static_cast<int>(i) == edge_view.edge_object_index) {
            continue;  // the qualified card's line became the object's main sentence + reading
          }
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
              if (edge_view.qualified && bright.probe_index_available != 0) {
                continue;  // the member-matched card's peak is the probe's own reading — the
                           // object's highest-bin reading owns it now, no echo
              }
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
        if (!edge_view.qualified) {
          lines.push_back("Wavelength dispersion: the " + model->dispersion.red_label + " inner edge sits " +
                          FormatDeg(model->dispersion.displacement_deg) + " inside the " +
                          model->dispersion.blue_label + " one (" + DirectionProse(model->dispersion.direction) + ").");
        }
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

// ---- The map overlay facts builder -------------------------------------------------------------------

namespace {

// The TIR turn's source grammar, the one the engine writes (dp_boundary's MarginName, fed by
// dp_weight_kink's margin = 2*step + 1): `internal_<step>_tir_discriminant`, where <step> is the
// reflection step's 1-based index along the representative slot sequence — NOT a crystal face
// number (the slot at that step is whatever face the path visits there). Anything else is not an
// internal-reflection turn the map can name — no fuzzy matching, no substring guessing.
bool ParseTirTurnStep(const std::string& source, int* step) {
  constexpr const char* kPrefix = "internal_";
  constexpr const char* kSuffix = "_tir_discriminant";
  if (source.size() <= std::strlen(kPrefix) + std::strlen(kSuffix)) {
    return false;
  }
  if (source.compare(0, std::strlen(kPrefix), kPrefix) != 0 ||
      source.compare(source.size() - std::strlen(kSuffix), std::strlen(kSuffix), kSuffix) != 0) {
    return false;
  }
  const std::string middle =
      source.substr(std::strlen(kPrefix), source.size() - std::strlen(kPrefix) - std::strlen(kSuffix));
  if (middle.empty() ||
      !std::all_of(middle.begin(), middle.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) {
    return false;
  }
  *step = std::atoi(middle.c_str());
  return true;
}

}  // namespace

SurveyOverlayFacts BuildSurveyOverlayFacts(const LUMICE_PathSurvey& result) {
  SurveyOverlayFacts facts;
  // The reach layer reads every delivered support interval, whichever object carries them (the
  // reader is the gate — no second kind table of ours), then merges the document's own adjacency:
  // an upper equal to the next lower is one continuous range, so no unreachable gap is invented
  // between exactly-touching intervals (C01's two intervals meet at their shared endpoint).
  std::vector<SurveyOverlayFacts::ReachInterval> raw;
  LUMICE_PathSurveyInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathSurveyGetInfo(&result, &info) == LUMICE_PATH_READ_OK) {
    for (size_t i = 0; i < info.object_count; ++i) {
      LUMICE_PathSurveySupport support{};
      support.struct_size = sizeof(support);
      if (LUMICE_PathSurveyGetSupport(&result, i, &support) != LUMICE_PATH_READ_OK) {
        continue;
      }
      for (size_t k = 0; k < support.interval_count; ++k) {
        LUMICE_PathSurveyInterval interval{};
        interval.struct_size = sizeof(interval);
        if (LUMICE_PathSurveyGetSupportInterval(&result, i, k, &interval) != LUMICE_PATH_READ_OK) {
          continue;
        }
        raw.push_back({ interval.lower_deg, interval.upper_deg });
      }
    }
  }
  if (!raw.empty()) {
    std::sort(raw.begin(), raw.end(),
              [](const SurveyOverlayFacts::ReachInterval& a, const SurveyOverlayFacts::ReachInterval& b) {
                return a.lower_deg < b.lower_deg;
              });
    SurveyOverlayFacts::ReachInterval merged = raw.front();
    for (size_t i = 1; i < raw.size(); ++i) {
      if (raw[i].lower_deg == merged.upper_deg) {
        // Exact adjacency (the document's own numbers, no epsilon): one continuous range.
        merged.upper_deg = std::max(merged.upper_deg, raw[i].upper_deg);
      } else if (raw[i].lower_deg > merged.upper_deg) {
        facts.reach_intervals.push_back(merged);
        merged = raw[i];
      } else {
        // Overlapping deliveries: the union's outer bound, never a fabricated interior edge.
        merged.upper_deg = std::max(merged.upper_deg, raw[i].upper_deg);
      }
    }
    facts.reach_intervals.push_back(merged);
    facts.reach_full_range = facts.reach_intervals.size() == 1 && facts.reach_intervals.front().lower_deg <= 0.0 &&
                             facts.reach_intervals.front().upper_deg >= 180.0;
  }
  // The edge layer reads each delivered edge card. The refused cards draw no arc (the conclusions'
  // rule); an unevaluated-but-delivered position draws the weakened dashed form (its claim did
  // not conclude); certified/finite draw solid.
  if (LUMICE_PathSurveyGetInfo(&result, &info) == LUMICE_PATH_READ_OK) {
    for (size_t i = 0; i < info.object_count; ++i) {
      LUMICE_PathSurveyObject obj{};
      obj.struct_size = sizeof(obj);
      if (LUMICE_PathSurveyGetObject(&result, i, &obj) != LUMICE_PATH_READ_OK ||
          obj.kind != LUMICE_PATH_SURVEY_CAUSTIC_EDGE || obj.state == LUMICE_PATH_STATE_REFUSED) {
        continue;
      }
      LUMICE_PathSurveyEdge edge{};
      edge.struct_size = sizeof(edge);
      if (LUMICE_PathSurveyGetEdge(&result, i, &edge) != LUMICE_PATH_READ_OK) {
        continue;
      }
      SurveyOverlayFacts::EdgeArc arc;
      arc.position_deg = edge.position_deg;
      arc.coincident_with_support = edge.coincident_with_support != 0;
      arc.certificate_available = edge.certificate_available != 0;
      arc.measure_limit_available = edge.measure_limit_available != 0;
      arc.measure_limit = edge.measure_limit;
      arc.concluded = obj.state == LUMICE_PATH_STATE_CERTIFIED || obj.state == LUMICE_PATH_STATE_FINITE;
      arc.object_index = i;
      facts.edges.push_back(arc);
    }
  }
  // The turn layer reads the diagnose features the document delivered: only a feature the engine
  // names visible AND names as an internal-reflection discriminant is a drawable turn. The
  // TIR quantity/share readers are calibers — they never enter this builder (no coordinates live
  // there, and none are invented).
  LUMICE_PathDiagnose diagnose{};
  diagnose.struct_size = sizeof(diagnose);
  if (LUMICE_PathSurveyGetDiagnose(&result, &diagnose) == LUMICE_PATH_READ_OK) {
    for (size_t i = 0; i < diagnose.feature_count; ++i) {
      LUMICE_PathDiagnoseFeature feature{};
      feature.struct_size = sizeof(feature);
      if (LUMICE_PathSurveyGetDiagnoseFeature(&result, i, &feature) != LUMICE_PATH_READ_OK || feature.visible == 0) {
        continue;
      }
      const std::string source = SurveyTextString(feature.source);
      int step = -1;
      if (!ParseTirTurnStep(source, &step)) {
        continue;
      }
      SurveyOverlayFacts::TirTurn turn;
      turn.source = source;
      turn.step = step;
      turn.delta_red_deg = feature.delta_red_deg;
      turn.delta_blue_deg = feature.delta_blue_deg;
      facts.tir_turns.push_back(std::move(turn));
    }
  }
  // The family arm's ordered curves (task arc-recovery-engine): the v4.66 readers, one row per
  // call, all borrowed arrays consumed immediately into plain values. A document without curves
  // (every non-family arm, an early stop) reads as an empty table — no curves, no fabrication.
  {
    LUMICE_PathSurveyCurveInfo curve_info{};
    curve_info.struct_size = sizeof(curve_info);
    if (LUMICE_PathSurveyGetCurves(&result, &curve_info) == LUMICE_PATH_READ_OK) {
      for (size_t c = 0; c < curve_info.curve_count; ++c) {
        LUMICE_PathSurveyCurve curve{};
        curve.struct_size = sizeof(curve);
        if (LUMICE_PathSurveyGetCurve(&result, c, &curve) != LUMICE_PATH_READ_OK) {
          continue;
        }
        SurveyOverlayFacts::FamilyCurveFact fact;
        fact.curve_index = static_cast<int>(c);
        fact.object_index = curve.object_index;
        fact.kind = curve.kind;
        fact.member.assign(curve.member, curve.member + curve.member_count);
        fact.level = curve.level;
        fact.closed = curve.closed != 0;
        fact.end_a = curve.end_a;
        fact.end_b = curve.end_b;
        fact.grid_spacing_deg[0] = curve.grid_spacing_deg[0];
        fact.grid_spacing_deg[1] = curve.grid_spacing_deg[1];
        fact.points.resize(curve.point_count);
        for (size_t p = 0; p < curve.point_count; ++p) {
          LUMICE_PathSurveyCurvePoint point{};
          point.struct_size = sizeof(point);
          if (LUMICE_PathSurveyGetCurvePoint(&result, c, p, &point) != LUMICE_PATH_READ_OK) {
            continue;  // a partial read leaves the default (out-of-sky) point — the draw skips it
          }
          SurveyOverlayFacts::FamilyCurveFact::Point& row = fact.points[p];
          row.params_deg[0] = point.params_deg[0];
          row.params_deg[1] = point.params_deg[1];
          for (int k = 0; k < 9; ++k) {
            row.pose[k] = point.pose[k];
          }
          row.alt_deg = point.image_altitude_deg;
          row.az_deg = point.image_azimuth_deg;
        }
        for (size_t s = 0; s < curve.segment_count; ++s) {
          LUMICE_PathSurveyCurveSegment segment{};
          segment.struct_size = sizeof(segment);
          if (LUMICE_PathSurveyGetCurveSegment(&result, c, s, &segment) != LUMICE_PATH_READ_OK) {
            continue;
          }
          SurveyOverlayFacts::FamilyCurveFact::Segment row;
          row.first = segment.first;
          row.last = segment.last;
          row.form = segment.form;
          row.image_alt_deg = segment.image_altitude_deg;
          row.image_az_deg = segment.image_azimuth_deg;
          row.image_diameter_deg = segment.image_diameter_deg;
          row.image_bound_deg = segment.image_bound_deg;
          fact.segments.push_back(row);
        }
        for (size_t e = 0; e < curve.cusp_count; ++e) {
          LUMICE_PathSurveyCurveCusp cusp{};
          cusp.struct_size = sizeof(cusp);
          if (LUMICE_PathSurveyGetCurveCusp(&result, c, e, &cusp) != LUMICE_PATH_READ_OK) {
            continue;
          }
          SurveyOverlayFacts::FamilyCurveFact::Cusp row;
          row.order_index = cusp.order_index;
          row.params_deg[0] = cusp.params_deg[0];
          row.params_deg[1] = cusp.params_deg[1];
          for (int k = 0; k < 9; ++k) {
            row.pose[k] = cusp.pose[k];
          }
          row.alt_deg = cusp.image_altitude_deg;
          row.az_deg = cusp.image_azimuth_deg;
          row.djv_residual = cusp.djv_residual;
          fact.cusps.push_back(row);
        }
        facts.family_curves.push_back(std::move(fact));
      }
    }
  }
  return facts;
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

std::string SurveyPartialBudgetNoteFor(const SurveyOverviewModel& model) {
  if (model.execution == LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED || model.early_stopped) {
    return "The survey stopped inside its budget — the facts on show are partial.";
  }
  return {};
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

SurveyOverviewModel BuildSurveyOverviewModel(const LUMICE_PathSurvey& result, bool point_sun_premise) {
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
    row.object_index = static_cast<int>(i);
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

  BuildInnerEdgeView(result, &model, point_sun_premise);
  BuildConclusions(result, &model);
  return model;
}

// ---- The renderer -----------------------------------------------------------------------------------

namespace {

// One labeled degree: the block texts keep three decimals, the unit is part of the number.
void TextDeg(const char* label, double deg) {
  ImGui::TextWrapped("%s %s", label, FormatDeg(deg).c_str());
}

// The row's addressable hover surface and THIS-frame's highlight (the §16.1 A2 pilot's row
// overlay, extended by the 2026-11-12 feedback wave: the row must also be locally perceivable —
// the owner overturned the "fully transparent, no highlight" form). An invisible button spanning
// the LAST item's own rect, submitted right after it; on hover a theme-interaction background is
// drawn UNDER the row's text — into the window's background channel, which the caller split for
// exactly this purpose (channels: 0 background, 1 content; the merge puts 0 under 1), so the
// text stays in the foreground and the highlight never mixes with a semantic colour. Plain text
// has no ID — the button carries both the hover affordance and the tests' addressing. Call
// immediately after the row's text item; the call site owns the row's identity write.
// `highlight_armed` is the interactive host's flag; without it the surface is the pilot's
// original fully transparent one and nothing is drawn.
bool HoverableRow(const char* id, bool highlight_armed) {
  const ImVec2 tmin = ImGui::GetItemRectMin();
  const ImVec2 tmax = ImGui::GetItemRectMax();
  ImGui::SetCursorScreenPos(tmin);
  ImGui::InvisibleButton(id, ImVec2(tmax.x - tmin.x, tmax.y - tmin.y));
  const bool hovered = ImGui::IsItemHovered();
  if (hovered && highlight_armed) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->ChannelsSetCurrent(0);
    dl->AddRectFilled(tmin, tmax, ImGui::GetColorU32(ImGuiCol_HeaderHovered));
    dl->ChannelsSetCurrent(1);
  }
  return hovered;
}

// The qualified inner-edge object's readings expander (the interactive host only — the summary
// hosts no folds). The four readings, each under its own evidence-level words (AC5): the
// geometric edge's certificate caliber, the profile cut's two finite-resolution readings with
// their shared representative/bins/λ premise, and the fixed chromatic pair. Unavailable readings
// name why — never a zero stand-in. While selected the group is forced open (the selected
// object's readings stay on show) and a one-shot scroll hint brings the entry into view.
void RenderInnerEdgeExpander(const SurveyInnerEdgeView& edge, SurveyInnerEdgeInteraction* inner_edge) {
  if (inner_edge->selected) {
    ImGui::SetNextItemOpen(true);  // the selected object's readings stay open while it is selected
  }
  const bool open = ImGui::CollapsingHeader("Inner edge readings##sv_inner_edge");
  if (!open) {
    return;
  }
  // Reading 1 — the geometric edge, the certificate's own evidence level.
  if (edge.certificate_available) {
    TextDeg("Geometric limit", edge.edge_deg);
    DisabledWrappedText(
        "Certified by the closed-form minimum — the support set's own lower bound (one stroke with it).");
  } else {
    TextDeg("Geometric edge", edge.edge_deg);
    DisabledWrappedText("Concluded without the closed-form certificate%s.",
                        edge.measure_limit_available ? "" : " and without a measure limit");
    if (edge.measure_limit_available) {
      DisabledWrappedText("Measure limit %s.", FormatDeg(edge.measure_limit).c_str());
    }
  }
  // Readings 2 and 3 — the profile cut's finite-resolution readings, stated at the bin's own
  // two-decimal precision (three decimals would overstate a 0.1° bin). Their shared premise (the
  // representative member, the actual bin width, the evaluated λ, the cut-only scope) rides once
  // beneath them: the highest bin is this cut's fact, never a row or all-sky peak claim.
  const bool any_bin = edge.first_bin.available || edge.highest_bin.available;
  if (edge.first_bin.available) {
    ImGui::TextWrapped("First occupied bin centre %s", FormatDeg2(edge.first_bin.deg).c_str());
  } else {
    DisabledWrappedText("First occupied bin centre: %s.", edge.first_bin.unavailable.c_str());
  }
  if (edge.highest_bin.available) {
    ImGui::TextWrapped("Highest bin centre on this cut %s", FormatDeg2(edge.highest_bin.deg).c_str());
  } else {
    DisabledWrappedText("Highest bin centre on this cut: %s.", edge.highest_bin.unavailable.c_str());
  }
  if (any_bin) {
    DisabledWrappedText(
        "The representative %s cut — %.1f° bins at %.0f nm, this profile only. Not the class sum, a "
        "whole-row or an all-sky peak.",
        edge.name.c_str(), edge.bin_deg, edge.wavelength_nm);
    if (edge.member_kind_available) {
      DisabledWrappedText(edge.member_kind == LUMICE_PATH_MEMBER_PHYSICAL_L2 ?
                              "Member scope: the physical class's members; this cut is its representative's." :
                              "Member scope: the literal written sequence.");
    }
  }
  // Reading 4 — the fixed chromatic pair, the engine's minimum-deviation row.
  if (edge.dispersion_available) {
    ImGui::TextWrapped("Fixed chromatic pair: %s at %s, %s at %s — %s apart (%s).", edge.dispersion_red_label.c_str(),
                       FormatDeg2(edge.dispersion_red_deg).c_str(), edge.dispersion_blue_label.c_str(),
                       FormatDeg2(edge.dispersion_blue_deg).c_str(), FormatDeg2(edge.dispersion_gap_deg).c_str(),
                       DirectionProse(edge.dispersion_direction).c_str());
    DisabledWrappedText("The minimum-deviation row on the representative %s — the fixed pair's own wavelengths.",
                        edge.name.c_str());
  } else {
    DisabledWrappedText("Fixed chromatic pair: %s.", edge.dispersion_unavailable.c_str());
  }
  // The profile's curve itself draws in the evidence fold (the one copy); the readings above
  // point at it rather than redrawing it.
  DisabledWrappedText("The cut's curve draws under Class evidence → Reference profile.");
  if (inner_edge->selected) {
    // The selection's map half: when the arc's layer is off, the object survives in the answer
    // and the map's silence is stated, never silent.
    if (!inner_edge->map_layer_draws) {
      DisabledWrappedText(
          "The map's reference layers are off — the selected edge is not drawn there; the selection "
          "is kept.");
    }
  }
}

// The default layer (the disposition ladder's L1/L2): the answers, their limits and their
// premises — everything that must stay readable with the fold groups closed (the coverage law:
// an L1 fact is never demoted into a fold for space). Shared verbatim by the complete answer
// (the Raypath view's class-answer host) and the main window's summary projection (§16.2: the
// summary is a read of the same layer, not a second wording). H1's gap readings — the diagnose
// verdict, the tint and TIR readings the conclusions don't already carry — read the same model
// fields here, so both hosts see them without a second model.
//
// `row_readout` is the interactive host's read: non-null, the hoverable rows (the two linked
// diagnose rows, the three profile reading rows and the ratio row) carry their addressable
// surfaces and their THIS-frame highlight feedback. Only the profile rows write an exclusive
// standing the canvas consumes next frame; verdict/Position and ratio feedback stays local.
// The highlight channel split wraps exactly the row-carrying body; the main-window summary
// passes null and renders byte-identically to its previous shape.
// `inner_edge` is the qualified object's entry wiring (the interactive host only): the main
// sentence's click surface, the selected highlight, the readings expander and the hidden-layer
// note. Null keeps the summary's shape.
void RenderSurveyDefaultLayer(const SurveyOverviewModel& model, SurveyRowHoverReadout* row_readout = nullptr,
                              SurveyInnerEdgeInteraction* inner_edge = nullptr) {
  // The interactive host's highlight channel: the window's own splitter, channel 1 carrying all
  // normal content, the hover backgrounds landing in channel 0 — the merge below puts them under
  // the text. No split on the summary host (null readout): no extra commands, byte-identical.
  if (row_readout != nullptr) {
    ImGui::GetWindowDrawList()->ChannelsSplit(2);
    ImGui::GetWindowDrawList()->ChannelsSetCurrent(1);
  }
  // The limits first (§16.1: state honesty covers both layers; the disabled block the header
  // used to carry keeps its content, minus the counts line that moved into the diagnostics
  // group).
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
  const std::string partial_budget_note = SurveyPartialBudgetNoteFor(model);
  if (!partial_budget_note.empty()) {
    ImGui::TextWrapped("%s", partial_budget_note.c_str());
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
  // minimu"). The qualified inner-edge object's main sentence leads (AC2: the entry reads
  // before the other structures); the interactive host adds its click surface — one click is
  // the selection request — and the persistent selected highlight on the same row. The premise
  // note rides beneath in both hosts: the whole-sky claim states its fixed λ, never a map
  // wavelength.
  size_t first_conclusion = 0;
  if (model.inner_edge.qualified && !model.conclusions.empty()) {
    first_conclusion = 1;
    ImGui::Bullet();
    ImGui::TextWrapped("%s", model.conclusions.front().c_str());
    const ImVec2 entry_min = ImGui::GetItemRectMin();
    const ImVec2 entry_max = ImGui::GetItemRectMax();
    if (row_readout != nullptr && inner_edge != nullptr) {
      // The entry surface over the sentence row itself (HoverableRow's placement discipline: the
      // button re-submits the text item's own rect, so the layout below is unchanged): one click
      // is the selection request; the persistent selected highlight and the transient hover
      // share the same channel discipline (background channel, under the text).
      ImGui::SetCursorScreenPos(entry_min);
      ImGui::InvisibleButton("###inner_edge_entry", ImVec2(entry_max.x - entry_min.x, entry_max.y - entry_min.y));
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        inner_edge->select_requested = true;
      }
      if (inner_edge->selected || ImGui::IsItemHovered()) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->ChannelsSetCurrent(0);
        dl->AddRectFilled(entry_min, entry_max,
                          ImGui::GetColorU32(inner_edge->selected ? ImGuiCol_Header : ImGuiCol_HeaderHovered));
        dl->ChannelsSetCurrent(1);
      }
      if (inner_edge->entry_request_scroll) {
        // The scroll request is the ENTRY's, consumed at the entry's own item (the click surface
        // is the current item here): the map-side commit brings THE OBJECT back into view, not a
        // later group's header — on a narrow column the wrapped sentence, premise and support
        // conclusions between the two can exceed half the viewport, and centering the header
        // would scroll the entry off. 0.0f puts the entry's own top at the viewport top, fully
        // visible whenever the viewport can hold it; the item's rect stands even when the item
        // is clipped away (LastItemData is the submitted rect, not the test registry's).
        inner_edge->entry_request_scroll = false;
        ImGui::SetScrollHereY(0.0f);
      }
    }
    DisabledWrappedText("%s", model.inner_edge.premise_note.c_str());
  }
  for (size_t i = first_conclusion; i < model.conclusions.size(); ++i) {
    ImGui::Bullet();
    ImGui::TextWrapped("%s", model.conclusions[i].c_str());
  }
  // The qualified object's readings expander: the interactive host's detail group, right where
  // the main sentence stands. The summary host renders no folds and no interaction.
  if (row_readout != nullptr && inner_edge != nullptr && model.inner_edge.qualified) {
    RenderInnerEdgeExpander(model.inner_edge, inner_edge);
  }
  // The chromatic block that ran neither half states so in the engine's own words — an
  // unevaluated leg is a fact on show, never a silent absence (R7).
  if (!model.dispersion.available && !model.diagnose.available && !model.chromatic_note.empty()) {
    DisabledWrappedText("Chromatic layer (%s): %s", SurveyStateName(model.chromatic_state),
                        model.chromatic_note.c_str());
  }
  // The colour verdict's main conclusion, default (the Dispersion/Diagnose row: 边位置、可见性/
  // 色彩主结论默认；coverage 不完整默认). Its notes and evidence basis stay in the evidence
  // group.
  // The verdict and Position rows keep addressable hover surfaces and local highlights in the
  // interactive host. They do not gate the persistent verdict anchor or write a session identity.
  // The coverage qualifier remains plain text; the anchor carries that limit in its dashed form.
  if (model.diagnose.available) {
    ImGui::Bullet();
    ImGui::TextWrapped("Weight colour: %s, %s, %s.", model.diagnose.verdict_kind.c_str(), model.diagnose.color.c_str(),
                       model.diagnose.visible ? "visible" : "not visible");
    if (row_readout != nullptr) {
      HoverableRow("###diagnose_verdict_row", true);
    }
    if (model.diagnose.position_available) {
      TextDeg("Position:", model.diagnose.position_deg);
      if (row_readout != nullptr) {
        HoverableRow("###diagnose_position_row", true);
      }
    }
    if (!model.diagnose.coverage_complete) {
      DisabledWrappedText("Coverage incomplete — the verdict names what it read, not the whole class.");
    }
  }
  // The tint reading for the arms whose conclusions don't already carry it (the identity arm's
  // conclusions quote the ratio; the caliber stays a Physics detail). A tint leg that ran but
  // produced no value names that at the default layer too — the leg's existence with an empty
  // value is an engine-reachable state, and dropping it silently would read the absence as if
  // the question had never been asked.
  if (model.tint.available && model.tint.reading_available && model.arm != SurveyArm::kIdentity) {
    ImGui::Bullet();
    ImGui::TextWrapped("Blue/red ratio %.3f — %s.", model.tint.ratio, DirectionProse(model.tint.direction).c_str());
    // The ratio row's hover surface and highlight are LOCAL only: the ratio names no spatial
    // payload, so it enters no session identity and drives no map geometry (the plan's honest
    // absence — a row feedback that cannot fabricate a location from a number it does not have).
    if (row_readout != nullptr) {
      HoverableRow("###tint_ratio_row", true);
    }
  } else if (model.tint.available && !model.tint.reading_available) {
    DisabledWrappedText("Weight tint: no reading (%s).", SurveyStateName(model.tint.state));
  }
  // The probe's refused mode is a limit — default at EVERY arm (§16.1's state honesty: a
  // refusal must not be readable only through a fold, and the full-dimensional arm's object
  // cards cannot speak for it). The readings stay arm-gated: the full-dimensional arm's
  // conclusions already carry the edge/peak facts, and repeating them here is the echo the
  // ledger deletes.
  if (model.probe.available && model.probe.state == LUMICE_PATH_STATE_REFUSED) {
    DisabledWrappedText("Probe refused (%s) — no readings claimed.", model.probe.mode.c_str());
  } else if (model.probe.available && model.arm != SurveyArm::kFullDimensional) {
    // The three reading rows: the profile device's trigger set (§16.1.3's extension). Each row
    // carries its hover surface and highlight; the standing is EXCLUSIVE — one pointer, one row
    // — and lands in the readout as the canvas's next-frame identity. The rows' own words stay
    // byte-identical: only the interaction surfaces are new.
    if (row_readout != nullptr &&
        (model.probe.edge_available || model.probe.peak_available || model.probe.separation_available)) {
      DisabledWrappedText("Hover profile readings to locate them on the map (Overlays on).");
    }
    if (model.probe.edge_available) {
      ImGui::Bullet();
      ImGui::TextWrapped("Profile edge %s", FormatDeg(model.probe.edge_deg).c_str());
      if (row_readout != nullptr && HoverableRow("###profile_edge_row", true)) {
        row_readout->profile = SurveyProfileHover::kEdge;
      }
    }
    if (model.probe.peak_available) {
      ImGui::Bullet();
      ImGui::TextWrapped("Profile peak %s", FormatDeg(model.probe.peak_deg).c_str());
      if (row_readout != nullptr && HoverableRow("###profile_peak_row", true)) {
        row_readout->profile = SurveyProfileHover::kPeak;
      }
    }
    if (model.probe.separation_available) {
      ImGui::Bullet();
      ImGui::TextWrapped("Peak–edge separation %s", FormatDeg(model.probe.separation_deg).c_str());
      if (row_readout != nullptr && HoverableRow("###profile_separation_row", true)) {
        row_readout->profile = SurveyProfileHover::kSeparation;
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
    DisabledWrappedText("Freezing ladder refused at level 0 (%s).", model.family.refusal_mode.c_str());
  }
  // The TIR share: the document-level reading and its refusal state, default; the interface
  // count is a Physics detail. Zero-shares read as the finite values they are.
  if (model.tir.available) {
    if (model.tir.value_available) {
      ImGui::Bullet();
      ImGui::TextWrapped("All-TIR share %.3f (red) / %.3f (blue).", model.tir.share_all_tir_red,
                         model.tir.share_all_tir_blue);
    } else if (model.tir.mode.empty()) {
      DisabledWrappedText("Internal-reflection share %s — no value and no named mode.",
                          SurveyStateName(model.tir.state));
    } else {
      DisabledWrappedText("Internal-reflection share %s (%s).", SurveyStateName(model.tir.state),
                          model.tir.mode.c_str());
    }
  }
  // The highlight channel's merge: the background channel (0) renders under the content channel
  // (1) — the hover backgrounds the rows drew. Only the interactive host ever split.
  if (row_readout != nullptr) {
    ImGui::GetWindowDrawList()->ChannelsMerge();
  }
}

// The class evidence group (the ladder's L3, the host's non-diagnostic fold): the optical
// premises, evidence methods and complete definitions behind the default layer's answers —
// nothing implementation-level, which lives in the env-gated diagnostics group below. One
// closed-by-default fold; each block keeps a small section label so the group stays navigable.
void RenderSurveyClassEvidence(const SurveyOverviewModel& model) {
  if (model.dispersion.available) {
    ImGui::TextDisabled("Chromatic dispersion");
    const SurveyDispersionView& d = model.dispersion;
    if (!d.min_row_available) {
      DisabledWrappedText("No minimum-deviation fold in this class — the followed onsets are the positional facts.");
    } else {
      ImGui::TextWrapped("%s inner edge %s — %s %s", d.red_label.c_str(), FormatDeg(d.value_red_deg).c_str(),
                         d.blue_label.c_str(), FormatDeg(d.value_blue_deg).c_str());
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
      DisabledWrappedText("Basis: %s", d.evidence_basis.c_str());
    }
    if (d.certificate_available) {
      DisabledWrappedText("Edge certificate: %s with A = %.3f°.", d.certificate_formula.c_str(),
                          d.certificate_apex_deg);
    }
  }
  if (model.diagnose.available) {
    ImGui::TextDisabled("Weight colour reading");
    for (const std::string& note : model.diagnose.notes) {
      DisabledWrappedText("%s", note.c_str());
    }
    if (!model.diagnose.evidence_basis.empty()) {
      DisabledWrappedText("Basis: %s", model.diagnose.evidence_basis.c_str());
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
        // The gap rule stays with the curve; the wavelength's n restatement (the engine's
        // dispersion table at this pair, the same value the default scope line already
        // carries) is a diagnostics-group item, not evidence.
        DisabledWrappedText("Deviation from %s to %s — empty bins are gaps, not zeros.", FormatDeg(d0).c_str(),
                            FormatDeg(d1).c_str());
      }
    }
  }
  if (model.tint.available && !model.tint.caliber.empty()) {
    ImGui::TextDisabled("Weight tint");
    DisabledWrappedText("Caliber: %s", model.tint.caliber.c_str());
  }
  if (model.witness.available) {
    ImGui::TextDisabled("Independent witness");
    if (model.witness.value_available) {
      ImGui::TextWrapped("Max kept weight %.4g.", model.witness.weight);
      DisabledWrappedText("Read on the declared source's own lattice — independent of the refused deviation axis.");
    } else {
      DisabledWrappedText("No readable witness value.");
    }
  }
  if (model.tir.available && model.tir.value_available) {
    ImGui::TextDisabled("Internal-reflection share");
    ImGui::TextWrapped("Read across %zu interface(s).", model.tir.interface_count);
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
        DisabledWrappedText("%s", row.reason.c_str());
      }
    }
    // The cap's truncation note stays AT the truncated list (the ladder's L2: the +N hint
    // lives where the cut happened).
    if (model.objects.size() > shown) {
      DisabledWrappedText("+ %zu more object(s) — beyond this view.", model.objects.size() - shown);
    }
  }
}

// The class diagnostics group: the dev-visible half, rendered only while
// LUMICE_ANALYZE_DEBUG_DETAILS is set (env::AnalyzeDebugDetails gates the header itself, so
// with the env off neither header nor body exists). The numeric and execution facts a check
// reads — counts, budgets, tolerances — plus the three implementation-level rows the evidence
// group gave up (the engine's dispersion-table n pair, the profile's wavelength-n restatement
// and the freezing ladder's raw level/frozen/free enumeration); the ladder's consequence and
// the scope's own n stay at the default layer either way.
void RenderSurveyClassDiagnostics(const SurveyOverviewModel& model) {
  ImGui::TextWrapped("%s — %zu member(s), %zu object(s), budget %d ms%s", SurveyOutcomeName(model.execution),
                     model.member_count, model.object_count, model.budget_ms,
                     model.early_stopped ? ", early-stopped" : "");
  if (model.dispersion.certificate_available) {
    DisabledWrappedText("Edge certificate cross-check %.1e° against tolerance %.1e°.",
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
  if (model.dispersion.available && model.dispersion.min_row_available) {
    const SurveyDispersionView& d = model.dispersion;
    DisabledWrappedText("Refractive indices n(%s) = %.4f, n(%s) = %.4f — the engine's dispersion table at this pair.",
                        d.red_label.c_str(), d.n_red, d.blue_label.c_str(), d.n_blue);
  }
  if (model.probe.available && model.probe.state != LUMICE_PATH_STATE_REFUSED && !model.probe.sample_value.empty()) {
    DisabledWrappedText("n = %.4f at this wavelength.", model.probe.refractive_index);
  }
  if (model.family.available) {
    ImGui::TextDisabled("Freezing ladder");
    for (const SurveyFamilyLevelView& level : model.family.levels) {
      if (!level.frozen_dims.empty()) {
        std::string frozen = "L" + std::to_string(level.level) + " frozen: ";
        for (const std::string& dim : level.frozen_dims) {
          frozen += dim + "; ";
        }
        DisabledWrappedText("%s", frozen.c_str());
      }
      if (!level.free_dims.empty()) {
        std::string free_dims = "L" + std::to_string(level.level) + " free: ";
        for (const std::string& dim : level.free_dims) {
          free_dims += dim + "; ";
        }
        DisabledWrappedText("%s", free_dims.c_str());
      }
    }
  }
}

}  // namespace

void RenderSurveyOverviewModel(const SurveyOverviewModel& model, SurveyRowHoverReadout* row_readout,
                               SurveyInnerEdgeInteraction* inner_edge) {
  if (!model.valid) {
    ImGui::TextUnformatted("Survey document unreadable.");
    return;
  }
  RenderSurveyDefaultLayer(model, row_readout, inner_edge);
  ImGui::Spacing();
  if (FoldOpen(kSurveyClassEvidenceLabel, "sv_answer")) {
    RenderSurveyClassEvidence(model);
    ImGui::Spacing();
  }
  // The env gate precedes the header: with the diagnostics off, neither the "Class
  // diagnostics" header nor its body is submitted — the storage's old expansion state stays
  // untouched behind the absent header (env on again restores it within the session).
  if (env::AnalyzeDebugDetails() && FoldOpen(kSurveyClassDiagnosticsLabel, "sv_answer")) {
    RenderSurveyClassDiagnostics(model);
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
  DisabledWrappedText("The complete answer is in the %s.", kRaypathViewWindowTitle);
}

}  // namespace lumice::gui

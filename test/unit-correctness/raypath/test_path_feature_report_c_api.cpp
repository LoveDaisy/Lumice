// The public target-free report bridge: append-only request validation, opaque immutable result,
// and the shared length-query/fetch JSON buffer contract.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "include/lumice_base.h"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"

namespace {

constexpr const char* kSceneJson = R"({
  "crystal": [{
    "id": 1, "type": "prism", "shape": {"height": 1},
    "axis": {
      "zenith": {"type": "uniform", "mean": 0, "std": 360},
      "azimuth": {"type": "uniform", "mean": 0, "std": 360},
      "roll": {"type": "uniform", "mean": 0, "std": 360}
    }
  }],
  "filter": [],
  "scene": {
    "light_source": {"type": "sun", "altitude": 0, "azimuth": 0, "spectrum": "D65"},
    "ray_num": 1000, "max_hits": 7,
    "scattering": [{"prob": 0, "entries": [{"crystal": 1, "proportion": 1}]}]
  },
  "render": []
})";

struct SceneDeleter {
  void operator()(LUMICE_Scene* scene) const { LUMICE_SceneDestroy(scene); }
};
struct ReportDeleter {
  void operator()(LUMICE_PathFeatureReport* report) const { LUMICE_PathFeatureReportDestroy(report); }
};
using ScenePtr = std::unique_ptr<LUMICE_Scene, SceneDeleter>;
using ReportPtr = std::unique_ptr<LUMICE_PathFeatureReport, ReportDeleter>;

ScenePtr MakeScene() {
  LUMICE_Scene* scene = nullptr;
  EXPECT_EQ(LUMICE_SceneFromJson(kSceneJson, &scene), LUMICE_OK);
  return ScenePtr(scene);
}

// The wobbled plate: zenith and azimuth-roll read off the measure vocabulary, sun at 20. Its
// {1,2} basal chain holds a single P|B|D member — the light shape the ABI-contract cases that
// must run a full analysis (the prefix-defaults case) resolve onto.
ScenePtr MakePlateScene() {
  static const char* kPlateJson = R"({
  "crystal": [{
    "id": 1, "type": "prism", "shape": {"height": 0.3},
    "axis": {
      "zenith": {"type": "gauss", "mean": 90, "std": 0.3},
      "azimuth": {"type": "uniform", "mean": 0, "std": 360},
      "roll": {"type": "uniform", "mean": 0, "std": 360}
    }
  }],
  "filter": [],
  "scene": {
    "light_source": {"type": "sun", "altitude": 20, "azimuth": 0, "spectrum": "D65"},
    "ray_num": 1000, "max_hits": 7,
    "scattering": [{"prob": 0, "entries": [{"crystal": 1, "proportion": 1}]}]
  },
  "render": []
})";
  LUMICE_Scene* scene = nullptr;
  EXPECT_EQ(LUMICE_SceneFromJson(kPlateJson, &scene), LUMICE_OK);
  return ScenePtr(scene);
}

struct Request {
  int faces[3] = { 3, 5, 0 };
  int layers[1] = { 2 };
  LUMICE_PathFeatureReportRequest c{};

  Request() {
    c.struct_size = sizeof(c);
    c.crystal_id = 1;
    c.faces = faces;
    c.face_count = 2;
    c.layer_face_counts = layers;
    c.layer_count = 1;
    c.sample_count = 64;
    c.symmetry_bits_plus_one = 1;
  }
};

struct Outcome {
  LUMICE_ErrorCode code;
  ReportPtr report;
  std::string error;
};

Outcome Analyse(const LUMICE_Scene* scene, const LUMICE_PathFeatureReportRequest* request) {
  LUMICE_PathFeatureReport* raw = reinterpret_cast<LUMICE_PathFeatureReport*>(&raw);
  char error[512];
  std::memset(error, 'x', sizeof(error));
  const LUMICE_ErrorCode code = LUMICE_AnalyzePathFeatureReport(scene, request, &raw, error, sizeof(error));
  return { code, ReportPtr(raw), std::string(error) };
}

std::string Json(const LUMICE_PathFeatureReport* report) {
  size_t length = 0;
  EXPECT_EQ(LUMICE_PathFeatureReportToJson(report, nullptr, 0, &length), LUMICE_OK);
  std::string text(length + 1, '\0');
  size_t fetched = 0;
  EXPECT_EQ(LUMICE_PathFeatureReportToJson(report, text.data(), text.size(), &fetched), LUMICE_OK);
  EXPECT_EQ(fetched, length);
  text.resize(length);
  return text;
}

TEST(PathFeatureReportCApi, TypedReadersExposeObjectsEvidenceAndAbsenceWithoutParsing) {
  const ScenePtr scene = MakeScene();
  Request request;
  const Outcome outcome = Analyse(scene.get(), &request.c);
  ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
  LUMICE_PathReportInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathReportGetInfo(outcome.report.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.requested_outer_samples, 64u);
  EXPECT_EQ(info.execution, LUMICE_PATH_OUTCOME_COMPLETED);
  EXPECT_LE(info.coverage_count, nlohmann::json::parse(Json(outcome.report.get())).at("coverage").size());
  EXPECT_GT(info.declared_skip_count, 0u);
  LUMICE_PathReportRuling ruling{};
  ruling.struct_size = sizeof(ruling);
  ASSERT_EQ(LUMICE_PathReportGetRuling(outcome.report.get(), &ruling), LUMICE_PATH_READ_OK);
  EXPECT_EQ(ruling.issued, info.absence_issued);
  LUMICE_PathReportScope scope{};
  scope.struct_size = sizeof(scope);
  ASSERT_EQ(LUMICE_PathReportGetScope(outcome.report.get(), &scope), LUMICE_PATH_READ_OK);
  EXPECT_GT(scope.scene_identity.length, 0u);
  EXPECT_EQ(scope.raw_spectrum_kind, 2);
  EXPECT_EQ(scope.raw_illuminant, 3);  // the D65 scene
  ASSERT_GT(scope.layer_count, 0u);
  LUMICE_PathReportLayer layer{};
  layer.struct_size = sizeof(layer);
  ASSERT_EQ(LUMICE_PathReportGetLayer(outcome.report.get(), 0, &layer), LUMICE_PATH_READ_OK);
  EXPECT_EQ(layer.crystal_id, 1);
  EXPECT_EQ(layer.crystal_kind, 1);
  EXPECT_EQ(layer.shape_scalar_count, 10u);
  LUMICE_PathReportAxis axis{};
  axis.struct_size = sizeof(axis);
  ASSERT_EQ(LUMICE_PathReportGetLayerAxis(outcome.report.get(), 0, 0, &axis), LUMICE_PATH_READ_OK);
  EXPECT_GT(axis.distribution, 0);
  LUMICE_PathReportShapeScalar scalar{};
  scalar.struct_size = sizeof(scalar);
  ASSERT_EQ(LUMICE_PathReportGetLayerShape(outcome.report.get(), 0, 0, &scalar), LUMICE_PATH_READ_OK);
  EXPECT_EQ(std::string(scalar.name.data, scalar.name.length), "height");
  EXPECT_EQ(LUMICE_PathReportGetLayerShape(outcome.report.get(), 0, 1, &scalar), LUMICE_PATH_READ_NOT_AVAILABLE);
  LUMICE_PathRawSpectrumRow raw{};
  raw.struct_size = sizeof(raw);
  EXPECT_EQ(LUMICE_PathReportGetRawSpectrumRow(outcome.report.get(), 0, &raw), LUMICE_PATH_READ_INVALID_KIND);
  EXPECT_GT(scope.member_count, 0u);
  if (scope.member_count > 0) {
    LUMICE_PathFaceSequence member{};
    member.struct_size = sizeof(member);
    ASSERT_EQ(LUMICE_PathReportGetMember(outcome.report.get(), 0, &member), LUMICE_PATH_READ_OK);
    EXPECT_GE(member.face_count, 2u);
  }
  if (scope.spectrum_row_count > 0) {
    LUMICE_PathSpectralRow row{};
    row.struct_size = sizeof(row);
    EXPECT_EQ(LUMICE_PathReportGetSpectralRow(outcome.report.get(), 0, &row), LUMICE_PATH_READ_OK);
    EXPECT_GE(row.wavelength_nm, 350.0);
  }
  if (info.support_member_count > 0) {
    LUMICE_PathReportSupportMember support{};
    support.struct_size = sizeof(support);
    ASSERT_EQ(LUMICE_PathReportGetSupportMember(outcome.report.get(), 0, &support), LUMICE_PATH_READ_OK);
    EXPECT_NE(support.coverage, 0);
    EXPECT_NE(support.walk_status, 0);
    if (support.onset_count > 0) {
      LUMICE_PathReportOnset onset{};
      onset.struct_size = sizeof(onset);
      EXPECT_EQ(LUMICE_PathReportGetSupportOnset(outcome.report.get(), 0, 0, &onset), LUMICE_PATH_READ_OK);
    }
    if (support.constant_curve_count > 0) {
      LUMICE_PathReportCurve curve{};
      curve.struct_size = sizeof(curve);
      ASSERT_EQ(LUMICE_PathReportGetConstantCurve(outcome.report.get(), 0, 0, &curve), LUMICE_PATH_READ_OK);
      if (curve.wavelength_count > 0) {
        LUMICE_PathReportCurveWavelength wavelength{};
        wavelength.struct_size = sizeof(wavelength);
        EXPECT_EQ(LUMICE_PathReportGetCurveWavelength(outcome.report.get(), 0, 0, 0, &wavelength), LUMICE_PATH_READ_OK);
      }
    }
    if (support.interval_count > 0) {
      LUMICE_PathReportInterval interval{};
      interval.struct_size = sizeof(interval);
      EXPECT_EQ(LUMICE_PathReportGetSupportInterval(outcome.report.get(), 0, 0, &interval), LUMICE_PATH_READ_OK);
      EXPECT_LE(interval.lower_rad, interval.upper_rad);
    }
  }
  LUMICE_PathReportBudgets budgets{};
  budgets.struct_size = sizeof(budgets);
  if (info.orbit_count > 0) {
    LUMICE_PathReportOrbit orbit{};
    orbit.struct_size = sizeof(orbit);
    ASSERT_EQ(LUMICE_PathReportGetOrbit(outcome.report.get(), 0, &orbit), LUMICE_PATH_READ_OK);
    if (orbit.sample_count > 0) {
      LUMICE_PathReportOrbitSample sample{};
      sample.struct_size = sizeof(sample);
      EXPECT_EQ(LUMICE_PathReportGetOrbitSample(outcome.report.get(), 0, 0, &sample), LUMICE_PATH_READ_OK);
    }
  }
  if (info.support_family_count > 0) {
    LUMICE_PathReportSupportFamily family{};
    family.struct_size = sizeof(family);
    ASSERT_EQ(LUMICE_PathReportGetSupportFamily(outcome.report.get(), 0, &family), LUMICE_PATH_READ_OK);
    if (family.interval_count > 0) {
      LUMICE_PathReportInterval interval{};
      interval.struct_size = sizeof(interval);
      EXPECT_EQ(LUMICE_PathReportGetFamilyInterval(outcome.report.get(), 0, 0, &interval), LUMICE_PATH_READ_OK);
    }
    if (family.member_count > 0) {
      LUMICE_PathFaceSequence member{};
      member.struct_size = sizeof(member);
      EXPECT_EQ(LUMICE_PathReportGetFamilyMember(outcome.report.get(), 0, 0, &member), LUMICE_PATH_READ_OK);
    }
  }
  ASSERT_EQ(LUMICE_PathReportGetBudgets(outcome.report.get(), &budgets), LUMICE_PATH_READ_OK);
  EXPECT_EQ(budgets.completed_outer_samples, info.completed_outer_samples);
  const auto budget_wire = nlohmann::json::parse(Json(outcome.report.get())).at("budgets");
  EXPECT_EQ(budgets.enumeration_max_optical_evaluations, budget_wire.at("enumeration").at("max_optical_evaluations"));
  LUMICE_PathReportMcEvidence mc{};
  mc.struct_size = sizeof(mc);
  ASSERT_EQ(LUMICE_PathReportGetMcEvidence(outcome.report.get(), &mc), LUMICE_PATH_READ_OK);
  EXPECT_EQ(mc.completed_samples, info.completed_outer_samples);
  if (mc.unfinished_count > 0) {
    LUMICE_PathText unfinished{};
    EXPECT_EQ(LUMICE_PathReportGetMcUnfinished(outcome.report.get(), 0, &unfinished), LUMICE_PATH_READ_OK);
    EXPECT_GT(unfinished.length, 0u);
  }
  if (mc.limitation_count > 0) {
    LUMICE_PathText limitation{};
    EXPECT_EQ(LUMICE_PathReportGetMcLimitation(outcome.report.get(), 0, &limitation), LUMICE_PATH_READ_OK);
    EXPECT_GT(limitation.length, 0u);
  }
  if (mc.component_count > 0) {
    LUMICE_PathMcComponent component{};
    component.struct_size = sizeof(component);
    EXPECT_EQ(LUMICE_PathReportGetMcComponent(outcome.report.get(), 0, &component), LUMICE_PATH_READ_OK);
  }
  if (mc.positive_source_count > 0) {
    LUMICE_PathPositiveSource source{};
    source.struct_size = sizeof(source);
    EXPECT_EQ(LUMICE_PathReportGetPositiveSource(outcome.report.get(), 0, &source), LUMICE_PATH_READ_OK);
    if (source.coordinates_available) {
      EXPECT_GE(source.crystal_kind, 1);
      EXPECT_LE(source.crystal_kind, 2);
      EXPECT_GT(source.height, 0.0);
    }
  }
  ASSERT_GT(info.coverage_count, 0u);

  LUMICE_PathReportCoverage coverage{};
  coverage.struct_size = sizeof(coverage);
  ASSERT_EQ(LUMICE_PathReportGetCoverage(outcome.report.get(), 0, &coverage), LUMICE_PATH_READ_OK);
  EXPECT_GT(coverage.subject.length, 0u);
  EXPECT_EQ(LUMICE_PathReportGetCoverage(outcome.report.get(), info.coverage_count, &coverage),
            LUMICE_PATH_READ_INVALID_INDEX);
  EXPECT_EQ(coverage.subject.data, nullptr);

  LUMICE_PathReportObject object{};
  object.struct_size = sizeof(object);
  if (info.object_count > 0) {
    ASSERT_EQ(LUMICE_PathReportGetObject(outcome.report.get(), 0, &object), LUMICE_PATH_READ_OK);
    EXPECT_GT(object.kind.length, 0u);
    if (object.u_count) {
      EXPECT_NE(object.u, nullptr);
    }
    LUMICE_PathReportAnnotation annotation{};
    annotation.struct_size = sizeof(annotation);
    ASSERT_EQ(LUMICE_PathReportGetAnnotation(outcome.report.get(), 0, &annotation), LUMICE_PATH_READ_OK);
    EXPECT_NE(annotation.state, 0);
    LUMICE_PathReportVisibility visibility{};
    visibility.struct_size = sizeof(visibility);
    ASSERT_EQ(LUMICE_PathReportGetVisibility(outcome.report.get(), 0, &visibility), LUMICE_PATH_READ_OK);
    EXPECT_EQ(visibility.state, object.visibility);
    EXPECT_GT(visibility.evidence_form.length, 0u);
    LUMICE_PathReportChromatic chromatic{};
    chromatic.struct_size = sizeof(chromatic);
    ASSERT_EQ(LUMICE_PathReportGetChromatic(outcome.report.get(), 0, &chromatic), LUMICE_PATH_READ_OK);
    EXPECT_EQ(chromatic.assessed, object.chromatic_assessed);
    EXPECT_GT(chromatic.edge_min_shift_rad, 0.0);
    if (chromatic.feature_count > 0) {
      LUMICE_PathReportChromaticFeature feature{};
      feature.struct_size = sizeof(feature);
      EXPECT_EQ(LUMICE_PathReportGetChromaticFeature(outcome.report.get(), 0, 0, &feature), LUMICE_PATH_READ_OK);
    }
    if (chromatic.note_count > 0) {
      LUMICE_PathText note{};
      EXPECT_EQ(LUMICE_PathReportGetChromaticNote(outcome.report.get(), 0, 0, &note), LUMICE_PATH_READ_OK);
    }
  }
  EXPECT_EQ(LUMICE_PathReportGetObject(outcome.report.get(), info.object_count, &object),
            LUMICE_PATH_READ_INVALID_INDEX);
  EXPECT_EQ(object.kind.data, nullptr);
  if (info.produced_kind_count > 0) {
    LUMICE_PathText produced{};
    ASSERT_EQ(LUMICE_PathReportGetProducedKind(outcome.report.get(), 0, &produced), LUMICE_PATH_READ_OK);
    EXPECT_GT(produced.length, 0u);
  }

  LUMICE_PathReportRecord record{};
  record.struct_size = sizeof(record);
  if (info.record_count > 0) {
    ASSERT_EQ(LUMICE_PathReportGetRecord(outcome.report.get(), 0, &record), LUMICE_PATH_READ_OK);
    EXPECT_GT(record.kind.length, 0u);
    if (record.sky_point_count > 0) {
      LUMICE_PathReportSkyPoint point{};
      point.struct_size = sizeof(point);
      EXPECT_EQ(LUMICE_PathReportGetSkyPoint(outcome.report.get(), 0, 0, &point), LUMICE_PATH_READ_OK);
    }
    if (record.source_parameter_count > 0) {
      LUMICE_PathReportSourceParameter parameter{};
      parameter.struct_size = sizeof(parameter);
      EXPECT_EQ(LUMICE_PathReportGetSourceParameter(outcome.report.get(), 0, 0, &parameter), LUMICE_PATH_READ_OK);
      EXPECT_GT(parameter.name.length, 0u);
    }
  }
  const auto check_record = [&](size_t index) {
    ASSERT_EQ(LUMICE_PathReportGetRecord(outcome.report.get(), index, &record), LUMICE_PATH_READ_OK);
    LUMICE_PathRecordAdjuncts adjuncts{};
    adjuncts.struct_size = sizeof(adjuncts);
    ASSERT_EQ(LUMICE_PathReportGetRecordAdjuncts(outcome.report.get(), index, &adjuncts), LUMICE_PATH_READ_OK);
    if (record.field_point_count > 0) {
      LUMICE_PathFieldPoint point{};
      point.struct_size = sizeof(point);
      ASSERT_EQ(LUMICE_PathReportGetFieldPoint(outcome.report.get(), index, 0, &point), LUMICE_PATH_READ_OK);
      LUMICE_PathFieldJet jet{};
      jet.struct_size = sizeof(jet);
      EXPECT_EQ(LUMICE_PathReportGetFieldJet(outcome.report.get(), index, 0, 0, &jet), LUMICE_PATH_READ_OK);
      EXPECT_EQ(LUMICE_PathReportGetFieldJet(outcome.report.get(), index, 0, 5, &jet), LUMICE_PATH_READ_INVALID_KIND);
    }
    if (adjuncts.boundary_source_available) {
      LUMICE_PathDiagnosticSource source{};
      source.struct_size = sizeof(source);
      EXPECT_EQ(LUMICE_PathReportGetBoundarySource(outcome.report.get(), index, &source), LUMICE_PATH_READ_OK);
    }
    if (adjuncts.boundary_value_available) {
      LUMICE_PathDiagnosticValue value{};
      value.struct_size = sizeof(value);
      ASSERT_EQ(LUMICE_PathReportGetBoundaryValue(outcome.report.get(), index, &value), LUMICE_PATH_READ_OK);
      if (value.interface_count > 0) {
        LUMICE_PathDiagnosticInterface interface{};
        interface.struct_size = sizeof(interface);
        EXPECT_EQ(LUMICE_PathReportGetBoundaryInterface(outcome.report.get(), index, 0, &interface),
                  LUMICE_PATH_READ_OK);
      }
    }
  };
  for (size_t index = 0; index < info.record_count; ++index) {
    check_record(index);
  }
  EXPECT_EQ(LUMICE_PathReportGetRecord(outcome.report.get(), info.record_count, &record),
            LUMICE_PATH_READ_INVALID_INDEX);
  if (info.source_count > 0) {
    LUMICE_PathReportSource source{};
    source.struct_size = sizeof(source);
    EXPECT_EQ(LUMICE_PathReportGetSource(outcome.report.get(), 0, &source), LUMICE_PATH_READ_OK);
    EXPECT_LT(source.spectral_row, scope.spectrum_row_count);
  }
  LUMICE_PathUnattributedSummary unattributed{};
  unattributed.struct_size = sizeof(unattributed);
  ASSERT_EQ(LUMICE_PathReportGetUnattributedSummary(outcome.report.get(), &unattributed), LUMICE_PATH_READ_OK);
  EXPECT_EQ(unattributed.structure_count, info.unattributed_count);
  if (info.unattributed_count > 0) {
    LUMICE_PathUnattributed row{};
    row.struct_size = sizeof(row);
    EXPECT_EQ(LUMICE_PathReportGetUnattributed(outcome.report.get(), 0, &row), LUMICE_PATH_READ_OK);
    EXPECT_LT(row.record_index, info.record_count);
  }
  if (info.declared_skip_count > 0) {
    LUMICE_PathReportDeclaredSkip skipped{};
    skipped.struct_size = sizeof(skipped);
    ASSERT_EQ(LUMICE_PathReportGetDeclaredSkip(outcome.report.get(), 0, &skipped), LUMICE_PATH_READ_OK);
    EXPECT_GT(skipped.reason.length, 0u);
  }
  EXPECT_EQ(LUMICE_PathReportGetInfo(nullptr, &info), LUMICE_PATH_READ_NULL_ARG);
  EXPECT_EQ(info.object_count, 0u);
  EXPECT_EQ(info.struct_size, sizeof(info));
}

TEST(PathFeatureReportCApi, TypedRecordsMatchPopulatedReportAndRejectAbsentNestedPayloads) {
  const ScenePtr scene = MakeScene();
  Request request;
  request.c.symmetry_bits_plus_one = 0;
  const double wavelength = 550.0;
  request.c.wavelengths_nm = &wavelength;
  request.c.wavelength_count = 1;
  const Outcome outcome = Analyse(scene.get(), &request.c);
  ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
  const auto document = nlohmann::json::parse(Json(outcome.report.get()));
  const auto& records = document.at("mc_evidence").at("records");
  ASSERT_GT(records.size(), 0u);
  LUMICE_PathReportInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathReportGetInfo(outcome.report.get(), &info), LUMICE_PATH_READ_OK);
  ASSERT_EQ(info.record_count, records.size());
  const auto check_populated_record = [&](size_t index) {
    LUMICE_PathReportRecord record{};
    record.struct_size = sizeof(record);
    ASSERT_EQ(LUMICE_PathReportGetRecord(outcome.report.get(), index, &record), LUMICE_PATH_READ_OK);
    EXPECT_EQ(std::string(record.kind.data, record.kind.length), records[index].at("kind"));
    const std::string evidence = records[index].at("evidence").get<std::string>();
    if (evidence == "actual") {
      EXPECT_EQ(record.evidence, LUMICE_PATH_MC_ACTUAL);
    } else if (evidence == "candidate") {
      EXPECT_EQ(record.evidence, LUMICE_PATH_MC_CANDIDATE);
    } else if (evidence == "unfinished") {
      EXPECT_EQ(record.evidence, LUMICE_PATH_MC_UNFINISHED);
    } else {
      ADD_FAILURE() << "unrecognized record evidence: " << evidence;
    }
    EXPECT_EQ(record.equation, records[index].at("fixed_observation").at("equation"));
    EXPECT_EQ(record.contributor_fraction_available, records[index].contains("contributor_fraction_of_estimated_Y"));
    if (record.contributor_fraction_available) {
      EXPECT_EQ(record.contributor_fraction_of_estimated_y, records[index].at("contributor_fraction_of_estimated_Y"));
    }
    LUMICE_PathRecordAdjuncts adjuncts{};
    adjuncts.struct_size = sizeof(adjuncts);
    ASSERT_EQ(LUMICE_PathReportGetRecordAdjuncts(outcome.report.get(), index, &adjuncts), LUMICE_PATH_READ_OK);
    EXPECT_EQ(adjuncts.band_available, records[index].contains("band"));
    LUMICE_PathFieldBand band{};
    band.struct_size = sizeof(band);
    EXPECT_EQ(LUMICE_PathReportGetFieldBand(outcome.report.get(), index, &band),
              adjuncts.band_available ? LUMICE_PATH_READ_OK : LUMICE_PATH_READ_NOT_AVAILABLE);
    if (adjuncts.band_available) {
      for (size_t boundary = 0; boundary < 2; ++boundary) {
        EXPECT_EQ(band.boundary_count[boundary], records[index].at("band").at("boundaries")[boundary].size());
      }
    }
    LUMICE_PathCorridor corridor{};
    corridor.struct_size = sizeof(corridor);
    const auto diagnostic =
        LUMICE_PathReportGetCorridor(outcome.report.get(), index, LUMICE_PATH_DIAGNOSTIC_BOUNDARY, 0, 0, &corridor);
    EXPECT_EQ(diagnostic, adjuncts.boundary_value_available ? LUMICE_PATH_READ_OK : LUMICE_PATH_READ_NOT_AVAILABLE);
  };
  for (size_t index = 0; index < info.record_count; ++index) {
    check_populated_record(index);
  }
  ASSERT_GT(info.source_count, 0u);
  LUMICE_PathPositiveSource witness{};
  witness.struct_size = sizeof(witness);
  ASSERT_EQ(LUMICE_PathReportGetPositiveSource(outcome.report.get(), 0, &witness), LUMICE_PATH_READ_OK);
  ASSERT_LT(witness.token, info.source_count);
  LUMICE_PathReportSource source{};
  source.struct_size = sizeof(source);
  ASSERT_EQ(LUMICE_PathReportGetSource(outcome.report.get(), witness.token, &source), LUMICE_PATH_READ_OK);
  EXPECT_EQ(witness.sample_index, source.sample_index);
  EXPECT_EQ(witness.member_index, source.member_index);
  EXPECT_EQ(witness.spectral_row, source.spectral_row);
  const auto& source_wire = document.at("sources").at(std::to_string(witness.token));
  EXPECT_EQ(source.sample_index, source_wire.at("outer_sample"));
  EXPECT_EQ(source.member_index, source_wire.at("member"));
  EXPECT_EQ(source.spectral_row, source_wire.at("spectral_row"));

  LUMICE_PathDiagnosticValue diagnostic{};
  diagnostic.struct_size = sizeof(diagnostic);
  EXPECT_EQ(LUMICE_PathReportGetDiagnosticValue(outcome.report.get(), 0, 0, 0, 0, &diagnostic),
            LUMICE_PATH_READ_INVALID_KIND);
  EXPECT_EQ(LUMICE_PathReportGetDiagnosticValue(outcome.report.get(), info.record_count,
                                                LUMICE_PATH_DIAGNOSTIC_BOUNDARY, 0, 0, &diagnostic),
            LUMICE_PATH_READ_INVALID_INDEX);
}

TEST(PathFeatureReportCApi, RawDiscreteSpectrumRowsRemainDistinctFromResolvedRows) {
  auto document = nlohmann::json::parse(kSceneJson);
  document["scene"]["light_source"]["spectrum"] = nlohmann::json::array();
  document["scene"]["light_source"]["spectrum"].push_back({ { "wavelength", 550 }, { "weight", 1 } });
  const std::string scene_json = document.dump();
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(scene_json.c_str(), &raw_scene), LUMICE_OK);
  const ScenePtr scene(raw_scene);
  Request request;
  const Outcome outcome = Analyse(scene.get(), &request.c);
  ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
  LUMICE_PathReportScope scope{};
  scope.struct_size = sizeof(scope);
  ASSERT_EQ(LUMICE_PathReportGetScope(outcome.report.get(), &scope), LUMICE_PATH_READ_OK);
  ASSERT_EQ(scope.raw_spectrum_kind, 1);
  ASSERT_EQ(scope.raw_spectrum_count, 1u);
  LUMICE_PathRawSpectrumRow row{};
  row.struct_size = sizeof(row);
  ASSERT_EQ(LUMICE_PathReportGetRawSpectrumRow(outcome.report.get(), 0, &row), LUMICE_PATH_READ_OK);
  EXPECT_EQ(row.wavelength_nm, 550.0);
  EXPECT_EQ(row.weight, 1.0);
  EXPECT_EQ(LUMICE_PathReportGetRawSpectrumRow(outcome.report.get(), 1, &row), LUMICE_PATH_READ_INVALID_INDEX);
}

TEST(PathFeatureReportCApi, NullArgumentsAndPointersAreRejected) {
  const ScenePtr scene = MakeScene();
  Request request;
  LUMICE_PathFeatureReport* out = nullptr;
  EXPECT_EQ(LUMICE_AnalyzePathFeatureReport(nullptr, &request.c, &out, nullptr, 0), LUMICE_ERR_NULL_ARG);
  EXPECT_EQ(LUMICE_AnalyzePathFeatureReport(scene.get(), nullptr, &out, nullptr, 0), LUMICE_ERR_NULL_ARG);
  EXPECT_EQ(LUMICE_AnalyzePathFeatureReport(scene.get(), &request.c, nullptr, nullptr, 0), LUMICE_ERR_NULL_ARG);
  request.c.faces = nullptr;
  EXPECT_EQ(Analyse(scene.get(), &request.c).code, LUMICE_ERR_NULL_ARG);
  EXPECT_EQ(LUMICE_PathFeatureReportToJson(nullptr, nullptr, 0, nullptr), LUMICE_ERR_NULL_ARG);
  LUMICE_PathFeatureReportDestroy(nullptr);
}

TEST(PathFeatureReportCApi, StructSizeAndLayerShapeAreValidated) {
  const ScenePtr scene = MakeScene();
  Request short_request;
  short_request.c.struct_size = offsetof(LUMICE_PathFeatureReportRequest, sample_count);
  Outcome outcome = Analyse(scene.get(), &short_request.c);
  EXPECT_EQ(outcome.code, LUMICE_ERR_INVALID_VALUE);
  EXPECT_NE(outcome.error.find("struct_size"), std::string::npos);

  Request multi_layer;
  multi_layer.layers[0] = 1;
  outcome = Analyse(scene.get(), &multi_layer.c);
  EXPECT_EQ(outcome.code, LUMICE_ERR_INVALID_VALUE);
  EXPECT_NE(outcome.error.find("layer face counts"), std::string::npos);

  Request empty_path;
  empty_path.c.face_count = 0;
  empty_path.c.faces = nullptr;
  outcome = Analyse(scene.get(), &empty_path.c);
  EXPECT_EQ(outcome.code, LUMICE_ERR_INVALID_VALUE);
  EXPECT_NE(outcome.error.find("non-empty layer"), std::string::npos);

  Request no_layers;
  no_layers.c.layer_count = 0;
  outcome = Analyse(scene.get(), &no_layers.c);
  EXPECT_EQ(outcome.code, LUMICE_ERR_INVALID_VALUE);
  EXPECT_NE(outcome.error.find("one non-empty layer"), std::string::npos);
}

TEST(PathFeatureReportCApi, UnsupportedMultiCrystalStillRejectsInvalidInput) {
  const ScenePtr scene = MakeScene();
  const int faces[]{ 3, 5, 1, 3 };
  const int layers[]{ 2, 2 };
  Request request;
  request.c.faces = faces;
  request.c.face_count = 4;
  request.c.layer_face_counts = layers;
  request.c.layer_count = 2;
  auto outcome = Analyse(scene.get(), &request.c);
  ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
  const auto document = nlohmann::json::parse(Json(outcome.report.get()));
  EXPECT_EQ(document["outcome"], "unsupported_multicrystal");
  EXPECT_EQ(document["budgets"]["optical_evaluations"], 0);
  LUMICE_PathReportInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathReportGetInfo(outcome.report.get(), &info), LUMICE_PATH_READ_OK);
  ASSERT_EQ(info.requested_layer_count, 2u);
  LUMICE_PathFaceSequence first{};
  first.struct_size = sizeof(first);
  ASSERT_EQ(LUMICE_PathReportGetRequestedLayer(outcome.report.get(), 0, &first), LUMICE_PATH_READ_OK);
  ASSERT_EQ(first.face_count, 2u);
  EXPECT_EQ((std::vector<int>{ first.faces[0], first.faces[1] }),
            document["requested_path_layers"][0].get<std::vector<int>>());
  LUMICE_PathFaceSequence second{};
  second.struct_size = sizeof(second);
  ASSERT_EQ(LUMICE_PathReportGetRequestedLayer(outcome.report.get(), 1, &second), LUMICE_PATH_READ_OK);
  ASSERT_EQ(second.face_count, 2u);
  EXPECT_EQ((std::vector<int>{ second.faces[0], second.faces[1] }),
            document["requested_path_layers"][1].get<std::vector<int>>());
  EXPECT_EQ(LUMICE_PathReportGetRequestedLayer(outcome.report.get(), 2, &second), LUMICE_PATH_READ_INVALID_INDEX);
  EXPECT_EQ(second.face_count, 0u);
  LUMICE_PathReportScope scope{};
  scope.struct_size = sizeof(scope);
  EXPECT_EQ(LUMICE_PathReportGetScope(outcome.report.get(), &scope), LUMICE_PATH_READ_NOT_AVAILABLE);
  EXPECT_EQ(scope.scene_identity.data, nullptr);

  request.c.crystal_id = 99;
  outcome = Analyse(scene.get(), &request.c);
  EXPECT_EQ(outcome.code, LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(outcome.report, nullptr);
  EXPECT_NE(outcome.error.find("unknown_crystal_id"), std::string::npos);
  request.c.crystal_id = 1;
  request.c.scene_layer_plus_one = 99;
  EXPECT_EQ(Analyse(scene.get(), &request.c).code, LUMICE_ERR_INVALID_VALUE);
  request.c.scene_layer_plus_one = 0;
  request.c.budget_ms = -1;
  EXPECT_EQ(Analyse(scene.get(), &request.c).code, LUMICE_ERR_INVALID_VALUE);
  request.c.budget_ms = 0;
  const double wavelength = 550, weight = -1;
  request.c.wavelength_count = 1;
  request.c.wavelengths_nm = &wavelength;
  request.c.wavelength_weights = &weight;
  EXPECT_EQ(Analyse(scene.get(), &request.c).code, LUMICE_ERR_INVALID_VALUE);
}

TEST(PathFeatureReportCApi, SerializesOnceAndKeepsTheResultImmutable) {
  const ScenePtr scene = MakeScene();
  Request request;
  const Outcome outcome = Analyse(scene.get(), &request.c);
  ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
  ASSERT_NE(outcome.report, nullptr);
  EXPECT_EQ(outcome.error, "");
  const std::string first = Json(outcome.report.get());
  EXPECT_EQ(Json(outcome.report.get()), first);
  const nlohmann::json doc = nlohmann::json::parse(first);
  EXPECT_EQ(doc["schema"], "lumice.path-feature-report");
  EXPECT_EQ(doc["schema_version"], 3);
  EXPECT_EQ(doc["budgets"]["requested_outer_samples"], 64);
  EXPECT_EQ(doc["scope"]["spectrum"].size(), 33u);

  char small[8];
  size_t full_length = 0;
  ASSERT_EQ(LUMICE_PathFeatureReportToJson(outcome.report.get(), small, sizeof(small), &full_length), LUMICE_OK);
  EXPECT_EQ(std::strlen(small), sizeof(small) - 1);
  EXPECT_EQ(full_length, first.size());
}

TEST(PathFeatureReportCApi, AcceptsExplicitWavelengthsAndRejectsInvalidCounts) {
  const ScenePtr scene = MakeScene();
  Request request;
  const double wavelengths[2] = { 500.0, 600.0 };
  const double weights[2] = { 0.25, 0.75 };
  request.c.wavelengths_nm = wavelengths;
  request.c.wavelength_weights = weights;
  request.c.wavelength_count = 2;
  Outcome outcome = Analyse(scene.get(), &request.c);
  ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
  const nlohmann::json doc = nlohmann::json::parse(Json(outcome.report.get()));
  EXPECT_DOUBLE_EQ(doc["scope"]["spectrum"][0]["nm"].get<double>(), 500.0);
  EXPECT_DOUBLE_EQ(doc["scope"]["spectrum"][1]["source_weight"].get<double>(), 0.75);

  request.c.wavelengths_nm = nullptr;
  outcome = Analyse(scene.get(), &request.c);
  EXPECT_EQ(outcome.code, LUMICE_ERR_NULL_ARG);
}

TEST(PathFeatureReportCApi, StopsExpandedWorkAtTheExplicitBudget) {
  const ScenePtr scene = MakeScene();
  Request request;
  std::vector<double> wavelengths(LUMICE_PATH_FEATURE_REPORT_MAX_WAVELENGTH_COUNT, 550.0);
  request.c.wavelengths_nm = wavelengths.data();
  request.c.wavelength_count = static_cast<int>(wavelengths.size());
  request.c.sample_count = LUMICE_PATH_FEATURE_REPORT_MAX_SAMPLE_COUNT;

  request.c.max_optical_evaluations = 100;
  request.c.max_field_evaluations = 1;
  const Outcome outcome = Analyse(scene.get(), &request.c);
  ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
  const auto doc = nlohmann::json::parse(Json(outcome.report.get()));
  EXPECT_EQ(doc["outcome"], "partial");
  EXPECT_EQ(doc["budgets"]["optical_evaluations"], 100);
}

TEST(PathFeatureReportCApi, OldRequestPrefixUsesNewDefaultsWithoutReadingTheSuffix) {
  // A plate whose {1,2} basal chain holds a single P|B|D member: a prefix struct_size means
  // the appended bits field is not read, so the class resolves to the P|B|D default and the
  // assembly's cost scales with its member count — the light chain keeps this ABI-contract
  // case off the seconds scale. The subject is the prefix/defaults contract, not the crystal.
  const ScenePtr scene = MakePlateScene();
  Request request;
  static const int kBasalFaces[] = { 1, 2 };
  request.c.faces = kBasalFaces;
  request.c.face_count = 2;
  request.c.struct_size = offsetof(LUMICE_PathFeatureReportRequest, sample_count) + sizeof(int);
  request.c.budget_ms = -1;
  request.c.max_optical_evaluations = 1;
  const auto outcome = Analyse(scene.get(), &request.c);
  ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
  const auto doc = nlohmann::json::parse(Json(outcome.report.get()));
  EXPECT_EQ(doc["scope"]["budget_ms"], 15000);
  EXPECT_EQ(doc["budgets"]["max_optical_evaluations"], 4000000);
}

TEST(PathFeatureReportCApi, RequestedPhysicalMemberScopeIsNotSilentlyExpanded) {
  const ScenePtr scene = MakeScene();
  Request request;
  request.c.symmetry_bits_plus_one = 1;
  const auto outcome = Analyse(scene.get(), &request.c);
  ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
  const auto doc = nlohmann::json::parse(Json(outcome.report.get()));
  EXPECT_EQ(doc["scope"]["physical_members"].size(), 1u);
  EXPECT_EQ(doc["scope"]["layers"][0]["symmetry_bits"], 0);
  request.c.symmetry_bits_plus_one = 9;
  EXPECT_EQ(Analyse(scene.get(), &request.c).code, LUMICE_ERR_INVALID_VALUE);
}

}  // namespace

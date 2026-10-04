#include <lumice_analytic.h>
#include <stdio.h>

int main(void) {
  int version = LUMICE_ANALYTIC_GetApiVersion();
  printf("LUMICE_ANALYTIC_GetApiVersion=%d\n", version);
  /* Header/library mismatch check: the run-time check of doc/analytic-api.md section 8.1. */
  if (version != LUMICE_ANALYTIC_API_VERSION) {
    return 1;
  }

  /* One computation through the installed header's types: a regular prism, the 22-degree-halo
   * path 3-5, identity pose, horizontal sunlight entering face 3 at 40 degrees. */
  LUMICE_ANALYTIC_Crystal crystal = { 0 };
  crystal.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  crystal.height = 1.0;
  for (int i = 0; i < 6; i++) {
    crystal.face_distance[i] = 1.0;
  }
  const int faces[2] = { 3, 5 };
  const double incident[3] = { -0.76604444311897801, 0.64278760968653925, 0.0 };
  const double pose[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  LUMICE_ANALYTIC_PathEvaluation eval = { 0 };
  eval.struct_size = sizeof(eval);
  LUMICE_ANALYTIC_ErrorCode rc = LUMICE_ANALYTIC_EvaluatePath(&crystal, faces, 2, 1.31, incident, pose, &eval);
  const double* d = eval.outgoing_direction;
  /* Squared length, so the consumer needs no libm (-lm on Linux) for one check. */
  double length2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
  printf("LUMICE_ANALYTIC_EvaluatePath rc=%d valid=%d segments=%d\n", (int)rc, eval.valid, eval.segment_count);
  int ok = rc == LUMICE_ANALYTIC_OK && eval.valid == 1 && eval.segment_count == 3 && length2 > 1.0 - 1e-12 &&
           length2 < 1.0 + 1e-12 && eval.fresnel_transmission > 0.0 && eval.fresnel_transmission < 1.0;
  LUMICE_ANALYTIC_ReleasePathEvaluation(&eval);
  LUMICE_ANALYTIC_DiagnosticSource source = { 0 };
  source.crystal = crystal;
  source.refractive_index = 1.31;
  source.token = 17;
  for (int i = 0; i < 9; ++i) {
    source.pose[i] = pose[i];
  }
  for (int i = 0; i < 3; ++i) {
    source.incident[i] = incident[i];
  }
  LUMICE_ANALYTIC_DiagnosticResult diagnostics = { 0 };
  diagnostics.struct_size = sizeof(diagnostics);
  rc = LUMICE_ANALYTIC_EvaluateDiagnosticBatch(faces, 2, &source, 1, &diagnostics);
  ok = ok && rc == LUMICE_ANALYTIC_OK && diagnostics.optical_count == 1 && diagnostics.optical[0].entry_available &&
       diagnostics.optical[0].area > 0 && diagnostics.optical[0].direction_index_available &&
       diagnostics.optical[0].source.token == 17;
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&diagnostics);
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&diagnostics);
  /* Shared budgets return a processed prefix, not a row per unprocessed input. */
  LUMICE_ANALYTIC_DiagnosticSource rows[3] = { source, source, source };
  rows[1].pose[0] = 5;
  rc = LUMICE_ANALYTIC_CorrectDeviationBatch(faces, 2, rows, 3, 0, 1000, &diagnostics);
  ok = ok && rc == LUMICE_ANALYTIC_OK && diagnostics.optical_count == 0 && diagnostics.termination == 6 &&
       diagnostics.path_evaluations == 0;
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&diagnostics);
  rc = LUMICE_ANALYTIC_CorrectDeviationBatch(faces, 2, rows, 3, 4096, 1000, &diagnostics);
  ok = ok && rc == LUMICE_ANALYTIC_OK && diagnostics.optical_count == 3 && diagnostics.termination == 0 &&
       diagnostics.optical[0].solve_status == 0 && diagnostics.optical[1].input_status != 0 &&
       diagnostics.optical[2].solve_status == 0;
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&diagnostics);
  LUMICE_ANALYTIC_WeightedSkySample sample = { 0 };
  sample.direction[2] = 1;
  sample.xyz_weight[0] = sample.xyz_weight[1] = sample.xyz_weight[2] = 1;
  const double seed[3] = { 0, 0, 1 };
  rc = LUMICE_ANALYTIC_TraceWeightedSkyField(&sample, 1, seed, 0, 0, .02, 1, 100, 1000, &diagnostics);
  ok = ok && rc == LUMICE_ANALYTIC_OK && diagnostics.field_count == 1 && diagnostics.field[0].status == 0 &&
       diagnostics.field[0].jets[6] > 0 && diagnostics.field[0].jets[9] < 0;
  LUMICE_ANALYTIC_ReleaseDiagnosticResult(&diagnostics);
  printf("LUMICE_ANALYTIC diagnostic numerics ok=%d\n", ok);
  return ok ? 0 : 1;
}

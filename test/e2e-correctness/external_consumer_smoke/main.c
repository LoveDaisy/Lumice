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
  return ok ? 0 : 1;
}

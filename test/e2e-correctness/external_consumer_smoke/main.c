#include <stdio.h>

#include <lumice_analytic.h>

int main(void) {
  int version = LUMICE_ANALYTIC_GetApiVersion();
  printf("LUMICE_ANALYTIC_GetApiVersion=%d\n", version);
  /* Header/library mismatch check: the run-time check of doc/analytic-api.md section 8.1. */
  return version == LUMICE_ANALYTIC_API_VERSION ? 0 : 1;
}

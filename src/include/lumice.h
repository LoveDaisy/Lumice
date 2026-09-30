#ifndef LUMICE_H_
#define LUMICE_H_

// The engine's whole C API, gathered. Each capability has its own header, and this one only
// includes them, so an includer that has not moved to the specific header it uses keeps compiling.
// It declares nothing: scripts/check_header_split.py checks that, and that it lists every
// capability header (the engine's export surface, cmake/export_surfaces.cmake).
#include "lumice_base.h"
#include "lumice_editor.h"
#include "lumice_engine.h"
#include "lumice_raypath.h"
#include "lumice_render.h"
#include "lumice_scene.h"

#endif  // LUMICE_H_

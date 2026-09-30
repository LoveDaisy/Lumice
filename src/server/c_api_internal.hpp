#ifndef SERVER_C_API_INTERNAL_H_
#define SERVER_C_API_INTERNAL_H_

// Every internal declaration of the C API bridges, for the unit tests and the test-only
// liblumice_testapi hooks that reach them directly. The declarations themselves live one header
// per layer (c_api_{scene,render,engine}_internal.hpp) so that a bridge includes only the layers it
// is registered at; a bridge includes those, never this one. NOT part of the public C API: do not
// include it from src/gui/ or ship it to consumers.

#include "server/c_api_engine_internal.hpp"
#include "server/c_api_render_internal.hpp"
#include "server/c_api_scene_internal.hpp"

#endif  // SERVER_C_API_INTERNAL_H_

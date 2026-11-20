# Keep the machine-wide CPM sources immutable. Both the app and gui_test link this
# build-local widgets TU; a dependency upgrade must explicitly revalidate the patch.
set(_widgets_src "${IMGUI_DIR}/imgui_widgets.cpp")
file(READ "${_widgets_src}" _widgets)
string(REPLACE "\r\n" "\n" _widgets "${_widgets}")
set(_text_call "    RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb);")
string(FIND "${_widgets}" "${_text_call}" _call_offset)
if(_call_offset EQUAL -1)
  message(FATAL_ERROR "ImGui ButtonEx text call changed: revalidate SmallButton ink clipping")
endif()
# The complete upstream file is pinned too: context changes can invalidate the
# assumptions about baseline, alignment or ItemSize even if the call still exists.
string(SHA256 _widgets_sha "${_widgets}")
if(NOT _widgets_sha STREQUAL "29e09708f06544892cd5d29d485900cbfbad1f76f273563b062041d2d0ba9ee4")
  message(FATAL_ERROR "ImGui widgets source changed: revalidate SmallButton ink clipping")
endif()
file(READ "${CMAKE_CURRENT_LIST_DIR}/imgui_small_button_ink.inl" _text_replacement)
string(REPLACE "${_text_call}" "${_text_replacement}" _widgets "${_widgets}")
set(LUMICE_IMGUI_WIDGETS "${CMAKE_CURRENT_BINARY_DIR}/imgui_widgets.cpp")
# configure_file avoids recompiling a 500KB TU on a no-op configure.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/imgui_widgets.cpp.in" "${_widgets}")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/imgui_widgets.cpp.in" "${LUMICE_IMGUI_WIDGETS}" COPYONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_widgets_src}"
             "${CMAKE_CURRENT_LIST_DIR}/imgui_small_button_ink.inl")

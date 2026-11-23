include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.18
)
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.93.0-docking
)
FetchContent_MakeAvailable(SDL3 imgui)

add_library(bw_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
)
# The web has no SDL GPU backend: SDL_Renderer on WebGL instead (gpu_render_web.cpp).
if(EMSCRIPTEN)
  target_sources(bw_imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
  set(bw_gpu_render gpu_render_web.cpp)
else()
  target_sources(bw_imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp)
  set(bw_gpu_render gpu_render.cpp)
endif()
target_include_directories(bw_imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(bw_imgui PUBLIC SDL3::SDL3-static)

# The keycap legend font, embedded: Roboto Medium from Dear ImGui's misc/fonts.
file(READ "${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf" keycap_font_hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${keycap_font_hex}")
configure_file(keycap_font.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/keycap_font.cpp @ONLY)
# The BW-2001's key legends, embedded: Saira Expanded Bold (fonts/README.md).
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Saira-ExpandedBold.ttf" pet_font_hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${pet_font_hex}")
configure_file(pet_font.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/pet_font.cpp @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS fonts/Saira-ExpandedBold.ttf)
# The launcher's title, embedded: Audiowide (fonts/README.md).
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Audiowide-Regular.ttf" title_font_hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${title_font_hex}")
configure_file(title_font.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/title_font.cpp @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS fonts/Audiowide-Regular.ttf)

add_executable(bw_ui main.cpp gestures.cpp bench_input.cpp bench_setup.cpp tool_click.cpp ${bw_gpu_render} window_order.cpp analyzer_window.cpp board_look.cpp board_view.cpp chip_panel.cpp contents_page.cpp instrument_marks.cpp keyboard_capture.cpp
                     keyboard_window.cpp matrix_capture.cpp matrix_keyboard_window.cpp keycap.cpp board_keys.cpp manual_view.cpp meter_window.cpp scope_window.cpp
                     shelf.cpp shelf_drawings.cpp tv_view.cpp tv_window.cpp knob_window.cpp bench.cpp
                     manual_window.cpp brief_window.cpp debug_window.cpp question_popup.cpp launcher.cpp loaded.cpp board_tags.cpp title.cpp
                     ${CMAKE_CURRENT_BINARY_DIR}/keycap_font.cpp ${CMAKE_CURRENT_BINARY_DIR}/pet_font.cpp
                     ${CMAKE_CURRENT_BINARY_DIR}/title_font.cpp)
target_link_libraries(bw_ui PRIVATE bw_session bw_machine bw_manual bw_runner bw_view bw_imgui
                                    bw_warnings)

# The web build (the `web` preset): the frame loops kept, the browser given its turn in
# GpuRender::present (ASYNCIFY); the Runner's threads as Web Workers (shared memory: the page
# cross-origin isolated, web/coi-serviceworker.js); the Machines and datasheets packed into the
# page's file system at /machines and /datasheets, without their development files.
if(EMSCRIPTEN)
  set_target_properties(bw_ui PROPERTIES SUFFIX ".html")
  target_link_options(bw_ui PRIVATE
    -sASYNCIFY -sASYNCIFY_IGNORE_INDIRECT -sPTHREAD_POOL_SIZE=12 -sALLOW_MEMORY_GROWTH
    -sSTACK_SIZE=8MB -sENVIRONMENT=web,worker
    "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    "SHELL:--preload-file ${PROJECT_SOURCE_DIR}/machines@/machines"
    "SHELL:--preload-file ${PROJECT_SOURCE_DIR}/datasheets@/datasheets"
    "--exclude-file=*/docs" "--exclude-file=*/.history" "--exclude-file=*.kicad_pro"
    "--exclude-file=*.kicad_prl" "--exclude-file=*.dsn" "--exclude-file=*.lck"
    "--exclude-file=*.place" "--exclude-file=*.bak")
  set_property(TARGET bw_ui APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)
  configure_file(web/coi-serviceworker.js ${CMAKE_CURRENT_BINARY_DIR}/coi-serviceworker.js COPYONLY)
endif()

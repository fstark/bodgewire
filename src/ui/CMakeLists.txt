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
  GIT_TAG v1.93.0
)
FetchContent_MakeAvailable(SDL3 imgui)

add_library(bw_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(bw_imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(bw_imgui PUBLIC SDL3::SDL3-static)

add_executable(bw_ui main.cpp analyzer_window.cpp board_view.cpp chip_panel.cpp manual_view.cpp meter_window.cpp
                     net_panel.cpp scope_window.cpp side_panel.cpp time_text.cpp toolbar.cpp tv_view.cpp)
target_link_libraries(bw_ui PRIVATE bw_session bw_machine bw_manual bw_runner bw_view bw_imgui
                                    bw_warnings)

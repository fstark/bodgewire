include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(bw_tests
  asm/asm6502_test.cpp
  board/board_check_test.cpp
  board/boards_test.cpp
  board/copper_test.cpp
  board/footprint_test.cpp
  board/kicad_board_test.cpp
  board/kicad_netlist_test.cpp
  board/kicad_pcb_writer_test.cpp
  board/place_rules_test.cpp
  board/placer_test.cpp
  board/sexpr_test.cpp
  devices/bmp_test.cpp
  devices/led_glow_test.cpp
  devices/tv_input_test.cpp
  devices/tv_test.cpp
  instruments/logic_analyzer_test.cpp
  instruments/logic_probe_test.cpp
  instruments/multimeter_test.cpp
  instruments/oscilloscope_test.cpp
  machine/crack_test.cpp
  machine/electrical_test.cpp
  machine/knob_setting_test.cpp
  machine/machine_test.cpp
  machine/test_points.cpp
  machine/typing_test.cpp
  machine/press_test.cpp
  machine/puzzle_test.cpp
  machine/repair_test.cpp
  manual/manual_test.cpp
  manual/markdown_test.cpp
  machines/board_probe_test.cpp
  machines/board_repair_test.cpp
  machines/bus_load_test.cpp
  machines/bw1_model.cpp
  machines/bw1_rig.cpp
  machines/bw1_test.cpp
  machines/bw1_puzzles_test.cpp
  machines/bw1_model_test.cpp
  machines/bw1_monitor_test.cpp
  machines/bw1_selftest_test.cpp
  machines/bodge_wire_test.cpp
  machines/ball_test.cpp
  machines/chaser_test.cpp
  machines/counter_test.cpp
  machines/dice_test.cpp
  machines/heartbeat_test.cpp
  machines/led_computer_test.cpp
  machines/knob_test.cpp
  machines/logic_analyzer_test.cpp
  machines/manuals_test.cpp
  machines/message_test.cpp
  machines/typewriter_test.cpp
  machines/net_drivers_test.cpp
  machines/scope_meter_test.cpp
  machines/pin_faults_test.cpp
  machines/shorts_power_test.cpp
  machines/stopwatch_test.cpp
  machines/tennis_test.cpp
  machines/test_pattern_faults_test.cpp
  machines/test_pattern_test.cpp
  netlist/builder_test.cpp
  netlist/bw1_bench_test.cpp
  netlist/diagnostic_test.cpp
  netlist/external_rom_test.cpp
  netlist/net_pins_test.cpp
  netlist/parser_test.cpp
  netlist/ram_bench_test.cpp
  netlist/sample_test.cpp
  netlist/switch_bench_test.cpp
  netlist/units_test.cpp
  parts/catalogue_test.cpp
  parts/connector_test.cpp
  parts/digit_test.cpp
  parts/gate_package_test.cpp
  parts/keyboard_test.cpp
  parts/led_test.cpp
  parts/ls107_test.cpp
  parts/ls138_test.cpp
  parts/ls154_test.cpp
  parts/ls157_test.cpp
  parts/ls161_test.cpp
  parts/ls164_test.cpp
  parts/ls165_test.cpp
  parts/ls175_test.cpp
  parts/ls244_test.cpp
  parts/ls273_test.cpp
  parts/ls47_test.cpp
  parts/ls48_test.cpp
  parts/ls74_test.cpp
  parts/ls90_test.cpp
  parts/ls93_test.cpp
  parts/memory_block_test.cpp
  parts/mos6502_cases.cpp
  parts/mos6502_conformance_test.cpp
  parts/mos6502_core_test.cpp
  parts/mos6502_test.cpp
  parts/mos6520_test.cpp
  parts/mos6530_test.cpp
  parts/oscillator_test.cpp
  parts/paddle_test.cpp
  parts/power_on_reset_test.cpp
  parts/ram2114_test.cpp
  parts/ram6102_test.cpp
  parts/rom_contents_test.cpp
  parts/rom_test.cpp
  parts/shift_register_2518_test.cpp
  parts/shift_register_2524_test.cpp
  parts/switch_test.cpp
  parts/ttl100_test.cpp
  parts/ttl125_test.cpp
  parts/ttl145_test.cpp
  parts/ttl177_test.cpp
  parts/z80_stub_test.cpp
  runner/analyzer_consumer_test.cpp
  runner/dispatcher_test.cpp
  runner/led_consumer_test.cpp
  runner/meter_consumer_test.cpp
  runner/probe_consumer_test.cpp
  runner/runner_test.cpp
  runner/scope_consumer_test.cpp
  runner/spsc_ring_test.cpp
  runner/triple_buffer_test.cpp
  session/session_test.cpp
  sim/check_test.cpp
  sim/circuit_test.cpp
  sim/event_queue_test.cpp
  sim/logic_test.cpp
  sim/primitives_test.cpp
  sim/recorder_test.cpp
  sim/rng_test.cpp
  sim/simulator_test.cpp
  sim/switched_test.cpp
  sim/time_test.cpp
  sim/vcd_test.cpp
  view/board_scene_test.cpp
  view/digit_shape_test.cpp
  view/keyboard_layout_test.cpp
  view/time_text_test.cpp
  view/time_view_test.cpp
  view/viewport_test.cpp
)
target_link_libraries(bw_tests PRIVATE bw_asm bw_board bw_devices bw_instruments bw_machine bw_manual
                                       bw_runner bw_session bw_view bw_warnings
                                       Catch2::Catch2WithMain)
target_compile_definitions(bw_tests PRIVATE BW_NETLIST_DIR="${PROJECT_SOURCE_DIR}/netlists"
                                           BW_BOARD_DIR="${PROJECT_SOURCE_DIR}/boards"
                                           BW_FOOTPRINT_DIR="${PROJECT_SOURCE_DIR}/boards/footprints"
                                           BW_PUZZLE_DIR="${PROJECT_SOURCE_DIR}/puzzles"
                                           BW_ROOT_DIR="${PROJECT_SOURCE_DIR}"
                                           BW_ROM_DIR="${PROJECT_SOURCE_DIR}/roms"
                                           BW_6502_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data/6502"
                                           BW_6502_EXTERNAL_DIR="${PROJECT_SOURCE_DIR}/external/6502")

include(Catch)
catch_discover_tests(bw_tests)

# bw asm on every ROM program: its output must be the committed .rom.
file(GLOB programs CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/roms/*.asm)
foreach(program ${programs})
  get_filename_component(name ${program} NAME_WE)
  add_test(NAME asm_${name}
           COMMAND ${CMAKE_COMMAND} -DBW=$<TARGET_FILE:bw> -DSOURCE=${program}
                   -DEXPECTED=${PROJECT_SOURCE_DIR}/roms/${name}.rom
                   -DOUT=${CMAKE_CURRENT_BINARY_DIR}/asm_${name}.rom
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/asm_cli.cmake)
endforeach()

# KiCad's own design rule check on every committed Board; no kicad-cli, no tests.
find_program(BW_KICAD_CLI kicad-cli PATHS /Applications/KiCad/KiCad.app/Contents/MacOS)
if(BW_KICAD_CLI)
  file(GLOB boards CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/boards/*/*.kicad_pcb)
  foreach(board ${boards})
    get_filename_component(name ${board} NAME_WE)
    add_test(NAME drc_${name}
             COMMAND ${BW_KICAD_CLI} pcb drc --severity-error --exit-code-violations
                     -o ${CMAKE_CURRENT_BINARY_DIR}/${name}_drc.rpt ${board})
  endforeach()
  # A generated, unrouted Board: no DRC violation, only unconnected pads.
  file(GLOB rules CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/boards/*/*.place)
  foreach(rule ${rules})
    get_filename_component(name ${rule} NAME_WE)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/place_${name})
    add_test(NAME place_drc_${name}
             COMMAND ${CMAKE_COMMAND} -DBW=$<TARGET_FILE:bw> -DKICAD_CLI=${BW_KICAD_CLI}
                     -DNETLIST=${PROJECT_SOURCE_DIR}/netlists/${name}.bwn -DRULES=${rule}
                     -DOUT=${CMAKE_CURRENT_BINARY_DIR}/place_${name}/${name}.kicad_pcb
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/place_drc.cmake)
  endforeach()
endif()

# ctest run_leds: bw run --leds prints a line a lamp at the end of the run, an LED by its
# reference and a digit's lamps by their segment, and refuses a netlist without lamps.
execute_process(COMMAND ${BW} run ${MACHINES}/010-counter/puzzles/01-dark-led.bwp --time 100ms --leds
                OUTPUT_VARIABLE dark RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR NOT dark MATCHES "\nled D1 0\\.00\n")
  message(FATAL_ERROR "the Counter's Dark LED: expected `led D1 0.00` (${result}), got:\n${dark}")
endif()

# The Stopwatch: DS1's eight lamps, DS2 and DS3 without their unconnected decimal point.
execute_process(COMMAND ${BW} run ${MACHINES}/040-stopwatch/stopwatch.bwn --time 100ms --leds
                OUTPUT_VARIABLE digits RESULT_VARIABLE result)
string(REGEX MATCHALL "led DS[1-3]\\.(a|b|c|d|e|f|g|dp) [01]\\.[0-9][0-9]\n" lamps "${digits}")
list(LENGTH lamps count)
if(NOT result EQUAL 0 OR NOT count EQUAL 22 OR NOT digits MATCHES "led DS1\\.dp "
   OR digits MATCHES "led DS2\\.dp ")
  message(FATAL_ERROR "the Stopwatch: expected 22 lamp lines (${result}, ${count}), got:\n${digits}")
endif()

execute_process(COMMAND ${BW} run ${MACHINES}/050-test-pattern/test_pattern.bwn --time 1ms --leds
                OUTPUT_QUIET ERROR_VARIABLE errors RESULT_VARIABLE result)
if(NOT result EQUAL 1 OR NOT errors MATCHES "no LED or digit")
  message(FATAL_ERROR "a netlist without lamps: expected a refusal (${result}), got:\n${errors}")
endif()

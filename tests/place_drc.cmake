# ctest place_drc_<m>: bw place a Board into the build directory, then KiCad's DRC on it. An
# unrouted Board has unconnected pads by design, so the report, not the exit code, decides
# (--exit-code-violations would count them).
execute_process(COMMAND ${BW} place ${NETLIST} ${RULES} -o ${OUT} --force RESULT_VARIABLE placed)
if(NOT placed EQUAL 0)
  message(FATAL_ERROR "bw place failed (${placed})")
endif()
execute_process(COMMAND ${KICAD_CLI} pcb drc --severity-error -o ${OUT}.rpt ${OUT}
                RESULT_VARIABLE checked)
if(NOT checked EQUAL 0)
  message(FATAL_ERROR "kicad-cli pcb drc failed (${checked})")
endif()
file(READ ${OUT}.rpt report)
foreach(line "Found 0 DRC violations" "Found 0 Footprint errors")
  string(FIND "${report}" "${line}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "${OUT}.rpt lacks '${line}':\n${report}")
  endif()
endforeach()

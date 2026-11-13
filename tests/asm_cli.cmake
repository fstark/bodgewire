# ctest asm_<name>: bw asm a ROM program into the build directory and compare the output with
# the committed .rom.
execute_process(COMMAND ${BW} asm ${SOURCE} -o ${OUT} RESULT_VARIABLE assembled)
if(NOT assembled EQUAL 0)
  message(FATAL_ERROR "bw asm failed (${assembled})")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files ${OUT} ${EXPECTED} RESULT_VARIABLE same)
if(NOT same EQUAL 0)
  message(FATAL_ERROR "${EXPECTED} differs from bw asm's output ${OUT}: run bw asm again")
endif()

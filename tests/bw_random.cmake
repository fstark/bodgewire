# ctest bw_random: bw random makes the same Procedural Puzzle for the same seed, and its `fault`
# line under a `machine` line is a Puzzle bw run loads (P8d). OUT is a scratch directory.
set(script ${MACHINES}/010-counter/puzzles/random.bwr)
execute_process(COMMAND ${BW} random ${script} --seed 3 OUTPUT_VARIABLE first RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR NOT first MATCHES "^fault [^\n]+\ntries [0-9]+\ngolden_s [0-9.]+\nseconds [0-9.]+\n$")
  message(FATAL_ERROR "bw random: expected a fault line, tries, golden_s and seconds (${result}), got:\n${first}")
endif()
string(REGEX MATCH "^fault [^\n]+\ntries [0-9]+" made "${first}")
execute_process(COMMAND ${BW} random ${script} --seed 3 OUTPUT_VARIABLE second)
string(REGEX MATCH "^fault [^\n]+\ntries [0-9]+" again "${second}")
if(NOT made STREQUAL again)
  message(FATAL_ERROR "bw random --seed 3 twice:\n${made}\nthen\n${again}")
endif()

# The Counter through links, with the made Fault as its one Puzzle.
string(REGEX MATCH "^fault [^\n]+" line "${first}")
file(REMOVE_RECURSE ${OUT})
file(MAKE_DIRECTORY ${OUT}/puzzles)
file(CREATE_LINK ${MACHINES}/010-counter/counter.bwn ${OUT}/counter.bwn SYMBOLIC)
file(CREATE_LINK ${MACHINES}/010-counter/board ${OUT}/board SYMBOLIC)
file(WRITE ${OUT}/puzzles/01-made.bwp "machine counter\n${line}\n")
execute_process(COMMAND ${BW} run ${OUT}/puzzles/01-made.bwp --time 1ms
                OUTPUT_QUIET ERROR_VARIABLE errors RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "the made Puzzle does not load (${result}): ${line}\n${errors}")
endif()

execute_process(COMMAND ${BW} random ${MACHINES}/010-counter/puzzles/none.bwr
                OUTPUT_QUIET ERROR_VARIABLE errors RESULT_VARIABLE result)
if(NOT result EQUAL 1 OR NOT errors MATCHES "cannot read file")
  message(FATAL_ERROR "a missing file: expected a refusal (${result}), got:\n${errors}")
endif()

# Runs one scenario that must not pass: the command line must exit with RC (1: a check failed,
# 2: the input was refused) and its output must match every regular expression of EXPECT
# (separated by |), so the reason is pinned, not only the line.
# One file per scenario and implementation: the cases run side by side.
get_filename_component(stem "${SCENARIO}" NAME_WE)
execute_process(COMMAND "${CLI}" "${SCENARIO}" --out "${OUT}/${TAG}${stem}.json" --quiet
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(text "${out}${err}")
if(NOT status EQUAL RC)
    message(FATAL_ERROR "${SCENARIO} exited with ${status}, not ${RC}:\n${text}")
endif()
string(REPLACE "|" ";" pieces "${EXPECT}")
# ~ stands for the rest of one output line; a newline cannot travel in a -D argument.
string(REPLACE "~" "[^\n]*" pieces "${pieces}")
foreach(piece IN LISTS pieces)
    if(NOT text MATCHES "${piece}")
        message(FATAL_ERROR "${SCENARIO} did not print '${piece}':\n${text}")
    endif()
endforeach()

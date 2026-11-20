# cmake -DRUNS=<exe>|<exe>... -DEXPECT=<code> -P expect_no_client_exit.cmake
# Runs each live test on its simulated no-client path and fails unless every one exits EXPECT.
string(REPLACE "|" ";" _runs "${RUNS}")
foreach(_exe IN LISTS _runs)
    if(NOT EXISTS "${_exe}")
        get_filename_component(_target "${_exe}" NAME_WE)
        message(FATAL_ERROR "${_exe} is missing: build the target ${_target} first (cmake --build <tree> --target ${_target}).")
    endif()
    execute_process(COMMAND "${_exe}" --simulate-no-client RESULT_VARIABLE _code OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _code STREQUAL "${EXPECT}")
        message(FATAL_ERROR "${_exe} --simulate-no-client exited ${_code}, expected ${EXPECT}:\n${_out}${_err}")
    endif()
    message(STATUS "${_exe}: exit ${_code}")
endforeach()

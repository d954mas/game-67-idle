# Runs a Unity test program with ARGS (a planted defect) and requires every case named in CASES
# (separated by |) to fail: the defect must be caught by the very checks meant for it.
execute_process(COMMAND "${EXE}" ${ARGS} RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(text "${out}${err}")
if(status EQUAL 0)
    message(FATAL_ERROR "${EXE} ${ARGS} passed:\n${text}")
endif()
string(REPLACE "|" ";" cases "${CASES}")
foreach(name IN LISTS cases)
    if(NOT text MATCHES ":${name}:FAIL")
        message(FATAL_ERROR "${name} did not fail under ${ARGS}:\n${text}")
    endif()
endforeach()

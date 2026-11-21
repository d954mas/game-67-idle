# Runs one scenario in two separate processes and fails unless the metrics files are byte-equal.
# The files carry the scenario's name: the cases run side by side.
get_filename_component(stem "${SCENARIO}" NAME_WE)
foreach(run a b)
    execute_process(COMMAND "${CLI}" "${SCENARIO}" --out "${OUT}/${TAG}twice_${stem}_${run}.json" --quiet RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "run ${run} of ${SCENARIO} exited with ${status}")
    endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUT}/${TAG}twice_${stem}_a.json" "${OUT}/${TAG}twice_${stem}_b.json" RESULT_VARIABLE differ)
if(NOT differ EQUAL 0)
    message(FATAL_ERROR "two runs of ${SCENARIO} with one seed wrote different metrics")
endif()

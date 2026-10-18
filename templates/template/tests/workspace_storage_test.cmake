# cmake -P tests/workspace_storage_test.cmake
# Configure-level contract of cmake/GameWorkspaceStorage.cmake: which builds
# may carry a workspace save root, and which worktree a record applies to.
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/GameWorkspaceStorage.cmake")

if(GAME_WS_PROBE STREQUAL "release")
    game_workspace_storage_enabled(enabled "Release" ON OFF)
    return()
endif()

function(expect actual expected label)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "${label}: expected '${expected}', got '${actual}'")
    endif()
endfunction()

game_workspace_storage_enabled(enabled "Debug" ON OFF)
expect("${enabled}" ON "a Debug native build may use the workspace root")
game_workspace_storage_enabled(enabled "Debug" ON ON)
expect("${enabled}" OFF "a web build saves to its own origin, never a path")
game_workspace_storage_enabled(enabled "Release" OFF OFF)
expect("${enabled}" OFF "a Release build without the option stays clean")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -DGAME_WS_PROBE=release -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE release_result OUTPUT_QUIET ERROR_QUIET)
if(release_result EQUAL 0)
    message(FATAL_ERROR "a Release build that asks for the workspace root must fail to configure")
endif()

string(RANDOM LENGTH 12 suffix)
set(base "${CMAKE_CURRENT_BINARY_DIR}/workspace_storage_probe_${suffix}")
file(MAKE_DIRECTORY "${base}/studio" "${base}/elsewhere")
file(WRITE "${base}/workspace.json"
    "{\"schema\": \"ai_studio.feature_workspace.v1\", \"studioWorktree\": \"${base}/studio\", \"storageRoot\": \"${base}/storage\"}")
game_workspace_storage_root(root "${base}/studio" "demo")
expect("${root}" "${base}/storage/demo" "the recorded worktree gets the recorded root")

file(WRITE "${base}/workspace.json"
    "{\"schema\": \"ai_studio.feature_workspace.v1\", \"studioWorktree\": \"${base}/elsewhere\", \"storageRoot\": \"${base}/storage\"}")
game_workspace_storage_root(root "${base}/studio" "demo")
expect("${root}" "" "a record naming another worktree is ignored")

file(WRITE "${base}/workspace.json" "{\"schema\": \"something-else\"}")
game_workspace_storage_root(root "${base}/studio" "demo")
expect("${root}" "" "a foreign record is ignored")
file(REMOVE_RECURSE "${base}")

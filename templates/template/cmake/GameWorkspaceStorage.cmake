# A feature workspace (ai_studio/workspace/feature_workspaces.mjs) records a
# save root of its own next to its Studio worktree. Native builds made inside
# it default to that root, so two workspaces never load each other's saves.
if(NOT EMSCRIPTEN AND EXISTS "${GAME_REPO_ROOT}/../workspace.json")
    file(READ "${GAME_REPO_ROOT}/../workspace.json" _game_ws_record)
    string(JSON _game_ws_schema ERROR_VARIABLE _game_ws_error GET "${_game_ws_record}" schema)
    string(JSON _game_ws_studio ERROR_VARIABLE _game_ws_error GET "${_game_ws_record}" studioWorktree)
    string(JSON _game_ws_storage ERROR_VARIABLE _game_ws_storage_error GET "${_game_ws_record}" storageRoot)
    if(_game_ws_schema STREQUAL "ai_studio.feature_workspace.v1" AND NOT _game_ws_error AND NOT _game_ws_storage_error)
        file(REAL_PATH "${GAME_REPO_ROOT}" _game_ws_repo)
        file(TO_CMAKE_PATH "${_game_ws_studio}" _game_ws_studio)
        file(TO_CMAKE_PATH "${_game_ws_storage}" _game_ws_storage)
        if(EXISTS "${_game_ws_studio}")
            file(REAL_PATH "${_game_ws_studio}" _game_ws_studio)
        endif()
        set(_game_ws_repo_cmp "${_game_ws_repo}")
        set(_game_ws_studio_cmp "${_game_ws_studio}")
        if(WIN32)
            string(TOLOWER "${_game_ws_repo_cmp}" _game_ws_repo_cmp)
            string(TOLOWER "${_game_ws_studio_cmp}" _game_ws_studio_cmp)
        endif()
        # The record must name this very worktree: a stray workspace.json above
        # an ordinary checkout must not redirect its saves.
        if(_game_ws_repo_cmp STREQUAL _game_ws_studio_cmp AND IS_ABSOLUTE "${_game_ws_storage}")
            target_compile_definitions(${GAME_TARGET} PRIVATE
                GAME_WORKSPACE_STORAGE_ROOT="${_game_ws_storage}/${GAME_STORAGE_APP_ID}")
            message(STATUS "Workspace save root: ${_game_ws_storage}/${GAME_STORAGE_APP_ID}")
        endif()
    endif()
endif()

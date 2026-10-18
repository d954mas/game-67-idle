# A feature workspace (ai_studio/workspace/feature_workspaces.mjs) records a
# save root of its own next to its Studio worktree. Dev and DevAPI builds made
# inside it default to that root, so two workspaces never load each other's
# saves. A Release build never carries the path: it is a developer's directory.

# Sets <out> to the value for GAME_WORKSPACE_STORAGE_ROOT, or to "" when
# <repo_root> is not the worktree a valid workspace record names.
function(game_workspace_storage_root out repo_root app_id)
    set(${out} "" PARENT_SCOPE)
    if(NOT EXISTS "${repo_root}/../workspace.json")
        return()
    endif()
    file(READ "${repo_root}/../workspace.json" record)
    string(JSON schema ERROR_VARIABLE error GET "${record}" schema)
    string(JSON studio ERROR_VARIABLE studio_error GET "${record}" studioWorktree)
    string(JSON storage ERROR_VARIABLE storage_error GET "${record}" storageRoot)
    if(NOT schema STREQUAL "ai_studio.feature_workspace.v1" OR error OR studio_error OR storage_error)
        return()
    endif()
    file(TO_CMAKE_PATH "${studio}" studio)
    file(TO_CMAKE_PATH "${storage}" storage)
    if(NOT EXISTS "${studio}" OR NOT IS_ABSOLUTE "${storage}")
        return()
    endif()
    file(REAL_PATH "${repo_root}" repo)
    file(REAL_PATH "${studio}" studio)
    if(CMAKE_HOST_WIN32)
        string(TOLOWER "${repo}" repo)
        string(TOLOWER "${studio}" studio)
    endif()
    # The record must name this very worktree: a stray workspace.json above an
    # ordinary checkout must not redirect its saves.
    if(repo STREQUAL studio)
        set(${out} "${storage}/${app_id}" PARENT_SCOPE)
    endif()
endfunction()

# Sets <out> to whether this build may use the workspace root. <requested> is
# the GAME_WORKSPACE_STORAGE option, which no release configuration may set.
function(game_workspace_storage_enabled out build_type requested emscripten)
    if(requested AND build_type MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
        message(FATAL_ERROR "GAME_WORKSPACE_STORAGE is for dev and DevAPI builds; ${build_type} artifacts must not carry a workspace save path.")
    endif()
    if(requested AND NOT emscripten)
        set(${out} ON PARENT_SCOPE)
    else()
        set(${out} OFF PARENT_SCOPE)
    endif()
endfunction()

if(NOT CMAKE_SCRIPT_MODE_FILE)
    set(_game_ws_default OFF)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(_game_ws_default ON)
    endif()
    option(GAME_WORKSPACE_STORAGE "Default saves to the feature workspace's own save root (dev and DevAPI builds only)." ${_game_ws_default})
    game_workspace_storage_enabled(_game_ws_enabled "${CMAKE_BUILD_TYPE}" "${GAME_WORKSPACE_STORAGE}" "${EMSCRIPTEN}")
    if(_game_ws_enabled)
        game_workspace_storage_root(_game_ws_root "${GAME_REPO_ROOT}" "${GAME_STORAGE_APP_ID}")
        if(_game_ws_root)
            target_compile_definitions(${GAME_TARGET} PRIVATE GAME_WORKSPACE_STORAGE_ROOT="${_game_ws_root}")
            message(STATUS "Workspace save root: ${_game_ws_root}")
        endif()
    endif()
endif()

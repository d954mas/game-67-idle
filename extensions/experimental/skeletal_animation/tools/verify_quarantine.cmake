if(NOT DEFINED EXTENSION_ROOT)
    message(FATAL_ERROR "EXTENSION_ROOT is required")
endif()

file(TO_CMAKE_PATH "${EXTENSION_ROOT}" extension_root)
if(NOT extension_root MATCHES "/extensions/experimental/skeletal_animation$")
    message(FATAL_ERROR "skeletal proof must remain under extensions/experimental")
endif()

get_filename_component(repo_root "${EXTENSION_ROOT}/../../.." ABSOLUTE)
if(EXISTS "${repo_root}/extensions/skeletal_animation")
    message(FATAL_ERROR "legacy non-experimental skeletal path still exists")
endif()

set(original_payload
    "CMakeLists.txt"
    "docs/skinned_mesh_renderer_contract_v001.md"
    "include/skeletal_animation/nt_skeletal_animation.h"
    "include/skeletal_animation/nt_skeletal_mesh.h"
    "src/nt_skeletal_animation_ozz.cpp"
    "src/nt_skeletal_mesh_cpu.cpp"
    "tools/skeletal_animation_ozz_probe.c"
    "tools/skeletal_mesh_contract_probe.c")
foreach(relative_path IN LISTS original_payload)
    if(NOT EXISTS "${EXTENSION_ROOT}/${relative_path}")
        message(FATAL_ERROR "missing quarantined payload file: ${relative_path}")
    endif()
endforeach()

file(READ "${EXTENSION_ROOT}/README.md" readme)
foreach(required IN ITEMS
        "incomplete proof"
        "CPU-only"
        "Does not implement rendering"
        "game-ready lifecycle"
        "production performance claim")
    string(FIND "${readme}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "README is missing quarantine statement: ${required}")
    endif()
endforeach()

file(READ "${EXTENSION_ROOT}/docs/known-defects.md" defects)
foreach(required IN ITEMS
        "wrap_time"
        "before sample"
        "joint_names"
        "cross the C ABI"
        "use-after-free"
        "finiteness"
        "silent no-op")
    string(FIND "${defects}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "known-defects evidence is missing: ${required}")
    endif()
endforeach()

file(READ "${EXTENSION_ROOT}/CMakeLists.txt" cmake_source)
if(cmake_source MATCHES "add_compile_definitions|set\\(CMAKE_MSVC_RUNTIME_LIBRARY")
    message(FATAL_ERROR "experimental extension mutates directory/global compile runtime flags")
endif()
string(FIND "${cmake_source}" "NT_ENABLE_EXPERIMENTAL_SKELETAL_ANIMATION" opt_in)
if(opt_in EQUAL -1)
    message(FATAL_ERROR "explicit experimental opt-in guard is missing")
endif()

file(READ "${EXTENSION_ROOT}/include/skeletal_animation/nt_skeletal_mesh.h" mesh_header)
file(READ "${EXTENSION_ROOT}/src/nt_skeletal_mesh_cpu.cpp" mesh_source)
file(READ "${EXTENSION_ROOT}/src/nt_skeletal_animation_ozz.cpp" animation_source)
if(mesh_header MATCHES "nt_skeletal_mesh_instance_draw" OR
   mesh_source MATCHES "nt_skeletal_mesh_instance_draw")
    message(FATAL_ERROR "silent draw API must remain removed while renderer is absent")
endif()

# Preserve reproducible source evidence for accepted defects without executing
# hang/UAF/exception cases in the normal test process.
foreach(required IN ITEMS
        "while (time_seconds < 0.0f)"
        "while (time_seconds > duration_seconds)"
        "clip->models.resize"
        "std::vector")
    string(FIND "${animation_source}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "animation defect evidence changed: ${required}")
    endif()
endforeach()
if(animation_source MATCHES "has_sampled_pose|catch[ \\t]*\\(")
    message(FATAL_ERROR "defect ledger must be reviewed after pose/exception behavior changes")
endif()

foreach(required IN ITEMS
        "nt_skeletal_mesh_t *mesh = nullptr"
        "delete mesh"
        "src.weights[influence]"
        "mesh->joint_names.size()")
    string(FIND "${mesh_source}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "mesh defect evidence changed: ${required}")
    endif()
endforeach()
if(mesh_source MATCHES "std::isfinite|catch[ \\t]*\\(")
    message(FATAL_ERROR "defect ledger must be reviewed after finite/exception behavior changes")
endif()

file(GLOB_RECURSE default_discovery_files LIST_DIRECTORIES false
    "${repo_root}/features/*/feature.json"
    "${repo_root}/templates/*/CMakeLists.txt"
    "${repo_root}/games/*/CMakeLists.txt"
    "${repo_root}/ai_studio/*catalog*.json"
    "${repo_root}/ai_studio/*package*.mjs"
    "${repo_root}/ai_studio/*registry*.mjs"
    "${repo_root}/ai_studio/*discover*.mjs"
    "${repo_root}/.github/*.yaml"
    "${repo_root}/.github/*.yml")
foreach(path IN LISTS default_discovery_files)
    if(path MATCHES "[/\\\\](build|external|extensions)[/\\\\]")
        continue()
    endif()
    file(READ "${path}" discovery_source)
    if(discovery_source MATCHES "skeletal_animation|experimental_skeletal")
        message(FATAL_ERROR "experimental skeletal code leaked into default discovery: ${path}")
    endif()
endforeach()

message(STATUS "experimental skeletal quarantine contract: pass")

set(fixture "${CMAKE_CURRENT_BINARY_DIR}/cloth_fixtures.gen.h")
add_custom_command(OUTPUT "${fixture}"
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_cook.py" --emit "${fixture}"
    DEPENDS test_cook.py ../tools/garment_cook.py VERBATIM)
add_executable(test_garment_cloth test_cloth.c "${fixture}")
target_include_directories(test_garment_cloth PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(test_garment_cloth PRIVATE garment_cloth unity)
nt_set_warning_flags(test_garment_cloth)
nt_set_sanitizer_flags(test_garment_cloth)
add_test(NAME test_garment_cloth COMMAND test_garment_cloth)
add_executable(test_garment_render test_render.c)
target_link_libraries(test_garment_render PRIVATE garment_cloth unity)
nt_set_warning_flags(test_garment_render)
nt_set_sanitizer_flags(test_garment_render)
add_test(NAME test_garment_render COMMAND test_garment_render)
add_executable(test_garment_actor test_actor.c)
target_link_libraries(test_garment_actor PRIVATE garment_cloth nt_hash unity)
nt_set_warning_flags(test_garment_actor)
nt_set_sanitizer_flags(test_garment_actor)
add_test(NAME test_garment_actor COMMAND test_garment_actor)
# The Python writer makes the blob and the C initializers the view must reproduce bit for bit.
set(asset_fixture "${CMAKE_CURRENT_BINARY_DIR}/garment_asset_fixture.gen.h")
add_custom_command(OUTPUT "${asset_fixture}" "${CMAKE_CURRENT_BINARY_DIR}/garment_asset_fixture.garment"
        "${CMAKE_CURRENT_BINARY_DIR}/garment_asset_empty.garment"
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_asset.py" --emit "${asset_fixture}"
        "${CMAKE_CURRENT_BINARY_DIR}/garment_asset_fixture.garment" "${CMAKE_CURRENT_BINARY_DIR}/garment_asset_empty.garment"
    DEPENDS test_asset.py ../tools/garment_blob.py ../tools/garment_cook.py VERBATIM)
add_executable(test_garment_asset test_asset.c "${asset_fixture}")
target_include_directories(test_garment_asset PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(test_garment_asset PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(test_garment_asset PRIVATE garment_cloth unity)
nt_set_warning_flags(test_garment_asset)
nt_set_sanitizer_flags(test_garment_asset)
add_test(NAME test_garment_asset COMMAND test_garment_asset)
add_test(NAME test_garment_cook COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_cook.py")
add_test(NAME test_garment_blob COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_asset.py")
add_test(NAME test_garment_binding COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_binding.py")
add_test(NAME test_garment_paint COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_paint.py")
add_test(NAME test_garment_model COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_model.py")
add_test(NAME test_garment_proxy COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_proxy.py")
# The host stepped against the solver directly; the blob comes from the Python side of the same test.
set(host_fixture "${CMAKE_CURRENT_BINARY_DIR}/garment_host_fixture.garment")
add_custom_command(OUTPUT "${host_fixture}"
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_host.py" --emit "${host_fixture}"
    DEPENDS test_host.py ../tools/garment_blob.py ../tools/garment_cook.py VERBATIM)
set_source_files_properties("${host_fixture}" PROPERTIES HEADER_FILE_ONLY TRUE)
add_executable(test_garment_host test_host.c ../tools/host/garment_host.c "${host_fixture}")
target_compile_definitions(test_garment_host PRIVATE "GARMENT_HOST_FIXTURE=\"${host_fixture}\"" _CRT_SECURE_NO_WARNINGS)
target_link_libraries(test_garment_host PRIVATE garment_cloth nt_hash unity)
nt_set_warning_flags(test_garment_host)
nt_set_sanitizer_flags(test_garment_host)
add_test(NAME test_garment_host COMMAND test_garment_host)
set(host_tests test_garment_host)
if(TARGET garment_host)
    add_test(NAME test_garment_host_py COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_host.py"
        "$<TARGET_FILE:garment_host>")
    list(APPEND host_tests test_garment_host_py)
endif()
set_tests_properties(test_garment_actor test_garment_asset test_garment_blob test_garment_cloth test_garment_cook test_garment_binding
    test_garment_paint test_garment_model test_garment_proxy test_garment_render ${host_tests} PROPERTIES LABELS "core")

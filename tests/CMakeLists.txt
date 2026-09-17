add_executable(asset_paths_tests asset_paths_tests.cpp)
target_link_libraries(asset_paths_tests PRIVATE symocraft_assets)
if(MSVC)
    target_compile_options(asset_paths_tests PRIVATE /W4 /utf-8)
endif()
symocraft_stage_assets(asset_paths_tests)

add_test(NAME assets.contracts COMMAND asset_paths_tests)
add_test(NAME assets.from_source_directory COMMAND asset_paths_tests)
set_tests_properties(assets.from_source_directory PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
set_tests_properties(assets.contracts assets.from_source_directory PROPERTIES TIMEOUT 45)

add_test(NAME assets.game_preflight COMMAND SymoCraft --check-assets)
set_tests_properties(assets.game_preflight PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    TIMEOUT 15)

add_test(NAME assets.game_package
    COMMAND "${CMAKE_COMMAND}"
        "-DGAME_EXECUTABLE=$<TARGET_FILE:SymoCraft>"
        "-DSOURCE_ASSETS=${PROJECT_SOURCE_DIR}/assets"
        "-DPROBE_PARENT=${CMAKE_CURRENT_BINARY_DIR}/package-probes"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckAssetPackage.cmake")
set_tests_properties(assets.game_package PROPERTIES TIMEOUT 45)

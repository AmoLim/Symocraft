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

add_executable(player_math_tests player_math_tests.cpp)
target_link_libraries(player_math_tests PRIVATE glm::glm)

add_executable(batch_safety_tests batch_safety_tests.cpp)
target_link_libraries(batch_safety_tests PRIVATE symocraft_glad glm::glm symocraft_vendor_headers glfw)

add_executable(mesh_safety_tests mesh_safety_tests.cpp
    ../src/world/chunk.cpp
    ../src/world/chunk_manager.cpp
    ../include/MemoryAllocator/AmoBase.cpp)
target_link_libraries(mesh_safety_tests PRIVATE symocraft_glad glm::glm symocraft_vendor_headers glfw)

add_executable(block_config_tests block_config_tests.cpp ../src/world/block.cpp)
target_link_libraries(block_config_tests PRIVATE symocraft_yaml glm::glm symocraft_vendor_headers glfw)

add_executable(key_snapshot_tests key_snapshot_tests.cpp)

foreach(test_target IN ITEMS player_math_tests batch_safety_tests mesh_safety_tests block_config_tests key_snapshot_tests)
    target_include_directories(${test_target} PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_compile_features(${test_target} PRIVATE cxx_std_20)
    target_compile_options(${test_target} PRIVATE /W4 /utf-8)
    set_target_properties(${test_target} PROPERTIES CXX_EXTENSIONS OFF)
endforeach()

add_test(NAME player.math COMMAND player_math_tests)
add_test(NAME renderer.batch_safety COMMAND batch_safety_tests)
add_test(NAME world.mesh_safety COMMAND mesh_safety_tests)
add_test(NAME world.block_config COMMAND block_config_tests "${PROJECT_SOURCE_DIR}/assets/configs/blockFormats.yaml")
add_test(NAME input.key_snapshot COMMAND key_snapshot_tests)
set_tests_properties(player.math renderer.batch_safety world.mesh_safety world.block_config input.key_snapshot PROPERTIES TIMEOUT 45)

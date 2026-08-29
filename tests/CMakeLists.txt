# Lightweight test setup: plain executables using assert()-style checks
# registered with ctest, so the project has no hard dependency on GoogleTest
# for the prototype. Swap in find_package(GTest) here if/when the team
# standardizes on it - the test source files are already structured as
# independent, single-purpose translation units to make that swap easy.

foreach(test_name IN ITEMS
    test_calibration
    test_perspective
    test_bev_processor
    test_recovery
)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE bev-core)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

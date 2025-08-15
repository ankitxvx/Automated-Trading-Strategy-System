# Tests CMakeLists.txt

# Basic functionality tests
add_executable(basic_tests basic_tests.cpp)
target_link_libraries(basic_tests hft_simulator_static)

# Performance tests
add_executable(performance_tests performance_tests.cpp)
target_link_libraries(performance_tests hft_simulator_static)

# Add test targets
add_test(NAME basic_functionality COMMAND basic_tests)
add_test(NAME performance_check COMMAND performance_tests)

# Installation
install(TARGETS basic_tests performance_tests
        DESTINATION bin/tests)
# Examples CMakeLists.txt

# Simple market data example
add_executable(market_data_example market_data_example.cpp)
target_link_libraries(market_data_example hft_simulator_static)

# FIX protocol example
add_executable(fix_protocol_example fix_protocol_example.cpp)
target_link_libraries(fix_protocol_example hft_simulator_static)

# Performance benchmark
add_executable(performance_benchmark performance_benchmark.cpp)
target_link_libraries(performance_benchmark hft_simulator_static)

# Installation
install(TARGETS market_data_example fix_protocol_example performance_benchmark
        DESTINATION bin/examples)
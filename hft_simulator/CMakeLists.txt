cmake_minimum_required(VERSION 3.18)
project(HFTSimulator VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native")
    set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3 -march=native -flto")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
endif()

# Find required packages
find_package(Threads REQUIRED)

# Find Python for bindings
find_package(Python3 COMPONENTS Interpreter Development QUIET)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/cpp/include)

# Source files
set(HFT_SOURCES
    cpp/src/market_data_simulator.cpp
    cpp/src/fix_protocol.cpp
    cpp/src/thread_pool.cpp
)

# Header files
set(HFT_HEADERS
    cpp/include/types.h
    cpp/include/lockfree_queue.h
    cpp/include/market_data_simulator.h
    cpp/include/fix_protocol.h
    cpp/include/thread_pool.h
)

# Create static library
add_library(hft_simulator_static STATIC ${HFT_SOURCES} ${HFT_HEADERS})
target_link_libraries(hft_simulator_static Threads::Threads)

# Create shared library  
add_library(hft_simulator_shared SHARED ${HFT_SOURCES} ${HFT_HEADERS})
target_link_libraries(hft_simulator_shared Threads::Threads)

# Set library properties
set_target_properties(hft_simulator_static PROPERTIES OUTPUT_NAME hft_simulator)
set_target_properties(hft_simulator_shared PROPERTIES OUTPUT_NAME hft_simulator)

# Create Python bindings (optional)
find_package(pybind11 QUIET)
if(pybind11_FOUND)
    pybind11_add_module(hft_simulator_py 
        python/bindings.cpp
        ${HFT_SOURCES}
    )
    target_link_libraries(hft_simulator_py PRIVATE Threads::Threads)
    target_compile_definitions(hft_simulator_py PRIVATE VERSION_INFO=${PROJECT_VERSION})
    install(TARGETS hft_simulator_py DESTINATION ${Python3_SITEARCH})
else()
    message(WARNING "pybind11 not found, skipping Python bindings")
endif()

# Examples and tests
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build test programs" ON)

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS hft_simulator_static hft_simulator_shared
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(FILES ${HFT_HEADERS} DESTINATION include/hft_simulator)

# CPack configuration
set(CPACK_PACKAGE_NAME "HFTSimulator")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-Frequency Trading Simulator")
include(CPack)
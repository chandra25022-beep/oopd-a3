# MT25022 - J Chandra Harsha
# CMakeLists.txt for Pandemic Spread Simulation
# OOPD Assignment 3
# Cross-platform: Works on Windows, Linux, macOS

cmake_minimum_required(VERSION 3.14)
project(PandemicSimulation
    VERSION 1.0
    DESCRIPTION "Parallel and Asynchronous Simulation of Pandemic Spread"
    LANGUAGES CXX
)

# ===================== C++ Standard =====================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ===================== Compiler Flags =====================
if(MSVC)
    # MSVC flags
    add_compile_options(/W4 /O2 /EHsc)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    # GCC / Clang flags
    add_compile_options(-Wall -Wextra -O2)
endif()

# ===================== Threading =====================
find_package(Threads REQUIRED)

# ===================== Common Library =====================
# Build all shared source files as a static library
add_library(pandemic_common STATIC
    MT25022_Person.cpp
    MT25022_Grid.cpp
    MT25022_Intervention.cpp
    MT25022_ThreadPool.cpp
    MT25022_SnapshotWriter.cpp
    MT25022_Simulation.cpp
)
target_link_libraries(pandemic_common PUBLIC Threads::Threads)
target_include_directories(pandemic_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ===================== Main Simulation Executable =====================
add_executable(pandemic_sim MT25022_Main.cpp)
target_link_libraries(pandemic_sim PRIVATE pandemic_common)

# ===================== Parameter Sweep Executable =====================
add_executable(sweep MT25022_Sweep.cpp MT25022_ParameterSweep.cpp)
target_link_libraries(sweep PRIVATE pandemic_common)

# ===================== Test Executable =====================
add_executable(test_correctness MT25022_Test.cpp)
target_link_libraries(test_correctness PRIVATE pandemic_common)

# ===================== Filesystem Support =====================
# Some older compilers need explicit linking for <filesystem>
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(pandemic_common PUBLIC stdc++fs)
endif()

# ===================== Custom Targets =====================

# Run correctness test
add_custom_target(run_test
    COMMAND test_correctness --N 100 --max-days 50 --seed 42
    DEPENDS test_correctness
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running correctness tests (Part B verification)"
)

# Run sequential simulation
add_custom_target(run_sequential
    COMMAND pandemic_sim --mode sequential --N 200 --max-days 100 --csv seq_output.csv
    DEPENDS pandemic_sim
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running sequential simulation"
)

# Run threaded simulation
add_custom_target(run_threaded
    COMMAND pandemic_sim --mode thread_pool --threads 4 --chunks 8 --N 200 --max-days 100 --csv threaded_output.csv
    DEPENDS pandemic_sim
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running threaded simulation (thread_pool, T=4, C=8)"
)

# Run parameter sweep
add_custom_target(run_sweep
    COMMAND sweep --N 200 --max-days 100
        --sweep-betas 0.04,0.06,0.08,0.10,0.12
        --sweep-vaccines 0.0,0.1,0.2,0.3,0.5
        --sweep-seeds 42,43,44
        --sweep-output sweep_results.csv
    DEPENDS sweep
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running parameter sweep (Part C2)"
)

# ===================== Install =====================
install(TARGETS pandemic_sim sweep test_correctness
    RUNTIME DESTINATION bin
)

# ===================== Output Info =====================
message(STATUS "=== Pandemic Simulation Build ===")
message(STATUS "C++ Standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler:        ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "System:          ${CMAKE_SYSTEM_NAME}")
message(STATUS "================================")

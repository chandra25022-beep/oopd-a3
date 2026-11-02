// MT25022 - J Chandra Harsha
// Test program for Part B: Correctness verification
// Runs the simulation with different thread counts and chunk strategies,
// then verifies that all CSV outputs are identical to the sequential version.

#include "MT25022_Simulation.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <stdexcept>

/**
 * @brief Read entire file into string.
 */
std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

/**
 * @brief Compare two CSV files.
 * @return true if identical
 */
bool compareCSV(const std::string& file1, const std::string& file2) {
    try {
        std::string content1 = readFile(file1);
        std::string content2 = readFile(file2);
        return content1 == content2;
    } catch (const std::exception& e) {
        std::cerr << "Error comparing: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Run a single test configuration.
 */
bool runTest(int N, int maxDays, uint64_t seed, int threads, int chunks,
             const std::string& strategy, const std::string& mode,
             const std::string& refFile) {
    
    std::string csvFile = "test_T" + std::to_string(threads) + 
                          "_C" + std::to_string(chunks) + 
                          "_" + strategy + "_" + mode + ".csv";
    
    std::cout << "  Testing: mode=" << mode << " T=" << threads 
              << " C=" << chunks << " strategy=" << strategy << " ... ";
    
    try {
        SimParams params;
        params.N = N;
        params.maxDays = maxDays;
        params.seed = seed;
        params.numThreads = threads;
        params.numChunks = chunks;
        params.chunkStrategy = strategy;
        params.mode = mode;
        params.csvFile = csvFile;
        
        Simulation sim(params);
        sim.run();
        
        if (compareCSV(refFile, csvFile)) {
            std::cout << "PASS" << std::endl;
            // Clean up test file
            std::filesystem::remove(csvFile);
            return true;
        } else {
            std::cout << "FAIL (output differs from sequential)" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cout << "FAIL (exception: " << e.what() << ")" << std::endl;
        return false;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "======================================" << std::endl;
    std::cout << "MT25022 - Correctness Verification Test" << std::endl;
    std::cout << "======================================" << std::endl;
    
    // Test parameters
    int N = 100;  // Small grid for fast testing
    int maxDays = 50;
    uint64_t seed = 42;
    
    // Parse optional test parameters
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--N" && i + 1 < argc) N = std::stoi(argv[++i]);
        else if (arg == "--max-days" && i + 1 < argc) maxDays = std::stoi(argv[++i]);
        else if (arg == "--seed" && i + 1 < argc) seed = std::stoull(argv[++i]);
    }
    
    std::cout << "Test grid: " << N << "x" << N 
              << ", days: " << maxDays 
              << ", seed: " << seed << std::endl;
    
    // Step 1: Run sequential reference
    std::string refFile = "test_sequential_ref.csv";
    std::cout << "\n[1] Running sequential reference..." << std::endl;
    {
        SimParams params;
        params.N = N;
        params.maxDays = maxDays;
        params.seed = seed;
        params.mode = "sequential";
        params.csvFile = refFile;
        
        Simulation sim(params);
        sim.run();
    }
    
    // Step 2: Test all configurations
    std::vector<int> threadCounts = {1, 2, 3, 4, 7, 8, 16};
    std::vector<std::string> strategies = {"row_strip", "square_block", "interleaved"};
    std::vector<std::string> modes = {"thread_per_day", "thread_pool"};
    
    int totalTests = 0;
    int passedTests = 0;
    
    std::cout << "\n[2] Running parallel tests..." << std::endl;
    
    for (const auto& mode : modes) {
        std::cout << "\n--- Mode: " << mode << " ---" << std::endl;
        for (const auto& strategy : strategies) {
            for (int T : threadCounts) {
                int C = T; // Chunks = threads
                totalTests++;
                if (runTest(N, maxDays, seed, T, C, strategy, mode, refFile)) {
                    passedTests++;
                }
            }
        }
    }
    
    // Step 3: Test with C > T (more chunks than threads)
    std::cout << "\n--- Extra: C > T tests ---" << std::endl;
    for (int T : {2, 4}) {
        for (int C : {T * 2, T * 4}) {
            for (const auto& strategy : strategies) {
                totalTests++;
                if (runTest(N, maxDays, seed, T, C, strategy, "thread_pool", refFile)) {
                    passedTests++;
                }
            }
        }
    }
    
    // Cleanup reference file
    std::filesystem::remove(refFile);
    
    // Report
    std::cout << "\n======================================" << std::endl;
    std::cout << "Results: " << passedTests << "/" << totalTests << " tests passed" << std::endl;
    
    if (passedTests == totalTests) {
        std::cout << "ALL TESTS PASSED - Deterministic correctness verified!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED - Check output for details" << std::endl;
    }
    std::cout << "======================================" << std::endl;
    
    return (passedTests == totalTests) ? 0 : 1;
}

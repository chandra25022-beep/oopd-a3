// MT25022 - J Chandra Harsha
// Main entry point for the Pandemic Spread Simulation
// Supports Parts A (sequential), B (threaded), C1 (snapshots), D (performance)

#include "MT25022_Simulation.h"
#include <iostream>
#include <exception>

/**
 * @brief Main function - entry point for the pandemic simulation.
 * 
 * Parses command-line arguments and runs the simulation in the
 * specified mode (sequential, thread_per_day, or thread_pool).
 * 
 * Usage examples:
 *   Sequential:       ./pandemic_sim --mode sequential --N 1000 --max-days 200
 *   Thread per day:   ./pandemic_sim --mode thread_per_day --threads 4 --chunks 8
 *   Thread pool:      ./pandemic_sim --mode thread_pool --threads 4 --chunks 16
 *   With snapshots:   ./pandemic_sim --snapshot-interval 10 --async-snapshots
 *   With I/O latency: ./pandemic_sim --snapshot-interval 5 --io-latency 100
 */
int main(int argc, char* argv[]) {
    try {
        // Parse command line arguments
        SimParams params = parseArgs(argc, argv);
        
        // Create and run simulation
        Simulation sim(params);
        [[maybe_unused]] SimSummary summary = sim.run();
        
        return 0;
        
    } catch (const std::invalid_argument& e) {
        std::cerr << "ERROR (invalid argument): " << e.what() << std::endl;
        std::cerr << "Use --help for usage information." << std::endl;
        return 1;
    } catch (const std::out_of_range& e) {
        std::cerr << "ERROR (out of range): " << e.what() << std::endl;
        return 2;
    } catch (const std::runtime_error& e) {
        std::cerr << "ERROR (runtime): " << e.what() << std::endl;
        return 3;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 4;
    } catch (...) {
        std::cerr << "ERROR: Unknown exception occurred" << std::endl;
        return 5;
    }
}

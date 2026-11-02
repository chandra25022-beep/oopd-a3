// MT25022 - J Chandra Harsha
// Parameter Sweep entry point (Part C2)
// Runs multiple independent simulations with different parameters

#include "MT25022_Simulation.h"
#include "MT25022_ParameterSweep.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <exception>

/**
 * @brief Parse comma-separated double values.
 */
std::vector<double> parseDoubles(const std::string& s) {
    std::vector<double> vals;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, ',')) {
        vals.push_back(std::stod(token));
    }
    return vals;
}

/**
 * @brief Parse comma-separated uint64 values.
 */
std::vector<uint64_t> parseSeeds(const std::string& s) {
    std::vector<uint64_t> vals;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, ',')) {
        vals.push_back(std::stoull(token));
    }
    return vals;
}

/**
 * @brief Main entry for parameter sweep.
 * 
 * Usage:
 *   ./sweep --N 500 --max-days 200 \
 *           --sweep-betas 0.04,0.06,0.08,0.10,0.12 \
 *           --sweep-vaccines 0.0,0.1,0.2,0.3,0.4,0.5 \
 *           --sweep-seeds 42,43,44,45,46 \
 *           --sweep-output sweep_results.csv \
 *           --sweep-time-limit 300
 */
int main(int argc, char* argv[]) {
    try {
        SimParams baseParams;
        std::vector<double> betas = {0.04, 0.06, 0.08, 0.10, 0.12};
        std::vector<double> vaccines = {0.0, 0.1, 0.2, 0.3, 0.4, 0.5};
        std::vector<uint64_t> seeds = {42, 43, 44, 45, 46};
        std::string outputCsv = "sweep_results.csv";
        int timeLimit = 0;
        
        // Parse arguments
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            auto getNext = [&]() -> std::string {
                if (i + 1 >= argc) throw std::invalid_argument("Missing value for: " + arg);
                return argv[++i];
            };
            
            if (arg == "--N") { baseParams.N = std::stoi(getNext()); }
            else if (arg == "--max-days") { baseParams.maxDays = std::stoi(getNext()); }
            else if (arg == "--days-infectious") { baseParams.daysInfectious = std::stoi(getNext()); }
            else if (arg == "--mortality") { baseParams.mortality = std::stod(getNext()); }
            else if (arg == "--initial-infected") { baseParams.initialInfected = std::stoi(getNext()); }
            else if (arg == "--mask-factor") { baseParams.maskFactor = std::stod(getNext()); }
            else if (arg == "--mode") { baseParams.mode = getNext(); }
            else if (arg == "--threads") { baseParams.numThreads = std::stoi(getNext()); }
            else if (arg == "--chunks") { baseParams.numChunks = std::stoi(getNext()); }
            else if (arg == "--chunk-strategy") { baseParams.chunkStrategy = getNext(); }
            else if (arg == "--sweep-betas") { betas = parseDoubles(getNext()); }
            else if (arg == "--sweep-vaccines") { vaccines = parseDoubles(getNext()); }
            else if (arg == "--sweep-seeds") { seeds = parseSeeds(getNext()); }
            else if (arg == "--sweep-output") { outputCsv = getNext(); }
            else if (arg == "--sweep-time-limit") { timeLimit = std::stoi(getNext()); }
            else if (arg == "--help") {
                std::cout << "Usage: sweep [options]\n"
                    << "  --N <int>                Grid size (default: 1000)\n"
                    << "  --max-days <int>         Max days per run (default: 365)\n"
                    << "  --mode <str>             Execution mode (default: sequential)\n"
                    << "  --threads <int>          Threads per run (default: 1)\n"
                    << "  --sweep-betas <csv>      Beta values (default: 0.04,0.06,0.08,0.10,0.12)\n"
                    << "  --sweep-vaccines <csv>   Vaccine fractions (default: 0.0,0.1,...,0.5)\n"
                    << "  --sweep-seeds <csv>      Seeds (default: 42,43,44,45,46)\n"
                    << "  --sweep-output <file>    Output CSV (default: sweep_results.csv)\n"
                    << "  --sweep-time-limit <sec> Time limit (default: 0=none)\n";
                return 0;
            }
            else {
                throw std::invalid_argument("Unknown argument: " + arg);
            }
        }
        
        // Run the sweep
        ParameterSweep sweep(baseParams, betas, vaccines, seeds, outputCsv, timeLimit);
        auto results = sweep.run();
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}

// MT25022 - J Chandra Harsha
// Parameter Sweep - runs multiple simulations asynchronously

#ifndef MT25022_PARAMETERSWEEP_H
#define MT25022_PARAMETERSWEEP_H

#include "MT25022_Simulation.h"
#include "MT25022_CSVWriter.h"
#include <vector>
#include <future>
#include <chrono>
#include <functional>
#include <atomic>

/**
 * @brief Configuration for a single sweep run.
 */
struct SweepConfig {
    double beta;            ///< Transmission probability
    double vaccineFraction; ///< Vaccination fraction
    uint64_t seed;          ///< Random seed
    int runIndex;           ///< Index in the sweep
    
    SweepConfig(double b, double v, uint64_t s, int idx)
        : beta(b), vaccineFraction(v), seed(s), runIndex(idx) {}
};

/**
 * @brief Result from a single sweep run.
 */
struct SweepResult {
    int runIndex;
    double beta;
    double vaccineFraction;
    uint64_t seed;
    SimSummary summary;
    bool completed;
    std::string error;
};

/**
 * @brief Parameter sweep runner.
 * 
 * Runs many independent simulations with different parameter combinations,
 * using std::async for concurrency. Handles results as they complete,
 * supports time limits, and produces a results CSV.
 * 
 * Demonstrates async programming, exception handling, and composition.
 */
class ParameterSweep {
private:
    SimParams baseParams_;       ///< Base parameters (beta and v will be overridden)
    std::vector<SweepConfig> configs_; ///< All run configurations
    std::string outputCsv_;      ///< Output results CSV path
    int timeLimitSeconds_;       ///< Time limit (0 = unlimited)
    
    /**
     * @brief Execute a single simulation run.
     * @param config Run configuration
     * @param baseParams Base parameters
     * @return SweepResult
     */
    static SweepResult executeRun(const SweepConfig& config, SimParams baseParams);

public:
    /**
     * @brief Construct a parameter sweep.
     * @param baseParams    Base simulation parameters
     * @param betas         Values of beta to sweep
     * @param vacFractions  Values of vaccine fraction to sweep
     * @param seeds         Random seeds to use
     * @param outputCsv     Output CSV file path
     * @param timeLimitSec  Time limit in seconds (0 = no limit)
     */
    ParameterSweep(const SimParams& baseParams,
                   const std::vector<double>& betas,
                   const std::vector<double>& vacFractions,
                   const std::vector<uint64_t>& seeds,
                   const std::string& outputCsv = "sweep_results.csv",
                   int timeLimitSec = 0);
    
    /**
     * @brief Run all configured simulations.
     * 
     * Submits each run as an async task, handles results as they complete
     * (printing progress), and writes to the results CSV.
     * 
     * If a time limit is set, cancels runs that haven't started and
     * waits for running ones.
     * 
     * @return Vector of completed SweepResults
     */
    std::vector<SweepResult> run();
    
    /** @brief Get total number of runs. */
    int totalRuns() const;
};

#endif // MT25022_PARAMETERSWEEP_H

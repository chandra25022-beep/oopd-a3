// MT25022 - J Chandra Harsha
// Simulation engine - supports sequential, thread-per-day, and thread-pool modes

#ifndef MT25022_SIMULATION_H
#define MT25022_SIMULATION_H

#include "MT25022_Grid.h"
#include "MT25022_Intervention.h"
#include "MT25022_SnapshotWriter.h"
#include "MT25022_CSVWriter.h"
#include "MT25022_ThreadPool.h"
#include <string>
#include <memory>
#include <chrono>
#include <vector>
#include <functional>

/**
 * @brief Simulation parameters bundled together.
 */
struct SimParams {
    int N = 1000;              ///< Grid size
    double beta = 0.08;        ///< Transmission probability
    int daysInfectious = 7;    ///< Days a person is infectious
    double mortality = 0.01;   ///< Mortality rate
    int initialInfected = 10;  ///< Initially infected count
    double vaccineFraction = 0.0; ///< Fraction vaccinated
    double maskFactor = 1.0;   ///< Mask factor (1 = no masks)
    double lockdownLambda = 0.3;  ///< Lockdown reduction factor
    double lockdownThetaOn = 0.02;  ///< Lockdown trigger threshold
    double lockdownThetaOff = 0.005; ///< Lockdown lift threshold
    uint64_t seed = 42;        ///< Random seed
    int maxDays = 365;         ///< Maximum simulation days
    int numThreads = 1;        ///< Number of threads (1 = sequential)
    int numChunks = 0;         ///< Number of chunks (0 = same as threads)
    std::string chunkStrategy = "row_strip"; ///< Chunk strategy name
    std::string mode = "sequential"; ///< Execution mode: sequential, thread_per_day, thread_pool
    std::string csvFile = "output.csv"; ///< Output CSV file
    int snapshotInterval = 0;  ///< Snapshot every K days (0 = disabled)
    std::string snapshotDir = "snapshots"; ///< Snapshot output directory
    int maxPendingSnapshots = 4; ///< Max pending async snapshot writes (Q)
    int ioLatencyMs = 0;       ///< Simulated I/O latency
    bool asyncSnapshots = false; ///< Use async snapshot writing
    
    /**
     * @brief Validate all parameters.
     * @throws std::invalid_argument if any parameter is invalid
     */
    void validate() const;
    
    /** @brief Print parameters summary. */
    void print() const;
};

/**
 * @brief Result summary from a simulation run.
 */
struct SimSummary {
    double peakInfectedFraction = 0.0; ///< Peak infected fraction
    int peakDay = 0;                    ///< Day of peak infection
    int totalInfected = 0;              ///< Total people ever infected
    int totalDeaths = 0;                ///< Total deaths
    int numDays = 0;                    ///< Number of days simulated
    double elapsedMs = 0.0;             ///< Wall clock time in ms
    uint64_t finalChecksum = 0;         ///< Final grid checksum
};

/**
 * @brief Main simulation engine.
 * 
 * Composes Grid, InterventionManager, SnapshotWriter, and CSVWriter.
 * Supports three execution modes:
 *   1. Sequential (single-threaded)
 *   2. Thread-per-day (create new threads each day)
 *   3. Thread pool (reuse threads)
 * 
 * Demonstrates composition, polymorphism (chunk strategies),
 * templates, exception handling, and multi-threading.
 */
class Simulation {
private:
    SimParams params_;                              ///< Simulation parameters
    std::unique_ptr<Grid> grid_;                    ///< The simulation grid
    InterventionManager interventions_;             ///< Intervention manager
    std::unique_ptr<ChunkStrategy> chunkStrategy_;  ///< How to divide the grid
    double betaEff_;                                ///< Current effective beta
    
    /**
     * @brief Run one day sequentially.
     * @param day Current day
     * @return PopulationCounts for the day
     */
    PopulationCounts runDaySequential(int day);
    
    /**
     * @brief Run one day with thread-per-day approach.
     * Creates C threads, one per chunk, joins them all.
     * @param day Current day
     * @return PopulationCounts for the day
     */
    PopulationCounts runDayThreadPerDay(int day);
    
    /**
     * @brief Run one day using the thread pool.
     * Submits C tasks to the pool and waits for all.
     * @param day Current day
     * @param pool Reference to the thread pool
     * @return PopulationCounts for the day
     */
    PopulationCounts runDayThreadPool(int day, ThreadPool& pool);
    
    /**
     * @brief Precompute infection probability array P[0..8].
     * P[k] = 1 - (1 - betaEff)^k
     */
    void precomputeP(double P[9]) const;

public:
    /**
     * @brief Construct a simulation with the given parameters.
     * @param params Simulation parameters
     * @throws std::invalid_argument if parameters are invalid
     */
    explicit Simulation(const SimParams& params);
    
    /** @brief Destructor. */
    ~Simulation() = default;
    
    // Non-copyable but movable
    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;
    Simulation(Simulation&&) = default;
    Simulation& operator=(Simulation&&) = default;
    
    /**
     * @brief Run the full simulation.
     * @return SimSummary with results
     */
    SimSummary run();
    
    /**
     * @brief Get the current grid (for testing/inspection).
     */
    const Grid& getGrid() const;
    
    /**
     * @brief Get the parameters.
     */
    const SimParams& getParams() const;
};

/**
 * @brief Parse command line arguments into SimParams.
 * @param argc Argument count
 * @param argv Argument values
 * @return Parsed SimParams
 * @throws std::invalid_argument on parse errors
 */
SimParams parseArgs(int argc, char* argv[]);

#endif // MT25022_SIMULATION_H

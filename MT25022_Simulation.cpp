// MT25022 - J Chandra Harsha
// Simulation engine implementation

#include "MT25022_Simulation.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <future>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <cstring>

// =================== SimParams ===================

void SimParams::validate() const {
    if (N <= 0) throw std::invalid_argument("N must be > 0");
    if (beta < 0.0 || beta > 1.0) throw std::invalid_argument("beta must be in [0, 1]");
    if (daysInfectious <= 0) throw std::invalid_argument("daysInfectious must be > 0");
    if (mortality < 0.0 || mortality > 1.0) throw std::invalid_argument("mortality must be in [0, 1]");
    if (initialInfected < 0) throw std::invalid_argument("initialInfected must be >= 0");
    if (initialInfected > N * N) throw std::invalid_argument("initialInfected exceeds grid size");
    if (vaccineFraction < 0.0 || vaccineFraction > 1.0) throw std::invalid_argument("vaccineFraction must be in [0, 1]");
    if (maskFactor <= 0.0 || maskFactor > 1.0) throw std::invalid_argument("maskFactor must be in (0, 1]");
    if (maxDays <= 0) throw std::invalid_argument("maxDays must be > 0");
    if (numThreads <= 0) throw std::invalid_argument("numThreads must be > 0");
    if (snapshotInterval < 0) throw std::invalid_argument("snapshotInterval must be >= 0");
    if (maxPendingSnapshots < 1) throw std::invalid_argument("maxPendingSnapshots must be >= 1");
    if (ioLatencyMs < 0) throw std::invalid_argument("ioLatencyMs must be >= 0");
    
    if (mode != "sequential" && mode != "thread_per_day" && mode != "thread_pool") {
        throw std::invalid_argument("mode must be: sequential, thread_per_day, or thread_pool");
    }
    
    if (chunkStrategy != "row_strip" && chunkStrategy != "square_block" && 
        chunkStrategy != "interleaved") {
        throw std::invalid_argument("chunkStrategy must be: row_strip, square_block, or interleaved");
    }
}

void SimParams::print() const {
    std::cout << "=== Simulation Parameters ===" << std::endl;
    std::cout << "Grid size (N):        " << N << "x" << N << std::endl;
    std::cout << "Beta:                 " << beta << std::endl;
    std::cout << "Days infectious:      " << daysInfectious << std::endl;
    std::cout << "Mortality:            " << mortality << std::endl;
    std::cout << "Initially infected:   " << initialInfected << std::endl;
    std::cout << "Vaccine fraction:     " << vaccineFraction << std::endl;
    std::cout << "Mask factor:          " << maskFactor << std::endl;
    std::cout << "Lockdown lambda:      " << lockdownLambda << std::endl;
    std::cout << "Lockdown theta_on:    " << lockdownThetaOn << std::endl;
    std::cout << "Lockdown theta_off:   " << lockdownThetaOff << std::endl;
    std::cout << "Seed:                 " << seed << std::endl;
    std::cout << "Max days:             " << maxDays << std::endl;
    std::cout << "Mode:                 " << mode << std::endl;
    std::cout << "Threads:              " << numThreads << std::endl;
    std::cout << "Chunks:               " << (numChunks > 0 ? numChunks : numThreads) << std::endl;
    std::cout << "Chunk strategy:       " << chunkStrategy << std::endl;
    std::cout << "CSV output:           " << csvFile << std::endl;
    if (snapshotInterval > 0) {
        std::cout << "Snapshot interval:    " << snapshotInterval << " days" << std::endl;
        std::cout << "Snapshot mode:        " << (asyncSnapshots ? "async" : "blocking") << std::endl;
        std::cout << "Max pending:          " << maxPendingSnapshots << std::endl;
        std::cout << "I/O latency:          " << ioLatencyMs << " ms" << std::endl;
    }
    std::cout << "=============================" << std::endl;
}

// =================== Simulation ===================

Simulation::Simulation(const SimParams& params) 
    : params_(params),
      interventions_(params.maskFactor, params.lockdownLambda, 
                     params.lockdownThetaOn, params.lockdownThetaOff),
      betaEff_(params.beta * params.maskFactor) {
    
    params_.validate();
    
    if (params_.numChunks <= 0) {
        params_.numChunks = params_.numThreads;
    }
    
    // Create grid and initialize
    grid_ = std::make_unique<Grid>(params_.N, params_.seed);
    grid_->initialize(params_.vaccineFraction, params_.initialInfected);
    
    // Create chunk strategy
    chunkStrategy_ = createChunkStrategy(params_.chunkStrategy);
}

void Simulation::precomputeP(double P[9]) const {
    P[0] = 0.0;
    for (int k = 1; k <= 8; ++k) {
        P[k] = 1.0 - std::pow(1.0 - betaEff_, k);
    }
}

PopulationCounts Simulation::runDaySequential(int day) {
    double P[9];
    precomputeP(P);
    
    // Create a single chunk covering the entire grid
    std::vector<GridChunk> chunks = chunkStrategy_->divide(params_.N, params_.numChunks);
    
    PopulationCounts total;
    for (const auto& chunk : chunks) {
        PopulationCounts chunkCounts = grid_->updateChunk(
            chunk, day, P, params_.mortality, params_.daysInfectious);
        total += chunkCounts;
    }
    
    grid_->swapGrids();
    return total;
}

PopulationCounts Simulation::runDayThreadPerDay(int day) {
    double P[9];
    precomputeP(P);
    
    std::vector<GridChunk> chunks = chunkStrategy_->divide(params_.N, params_.numChunks);
    
    // Start one thread per chunk
    std::vector<std::thread> threads;
    std::vector<PopulationCounts> results(chunks.size());
    
    for (size_t i = 0; i < chunks.size(); ++i) {
        // Using lambda to capture chunk processing
        threads.emplace_back([this, &chunks, &results, i, day, &P]() {
            results[i] = grid_->updateChunk(
                chunks[i], day, P, params_.mortality, params_.daysInfectious);
        });
    }
    
    // Join all threads (only coordination mechanism used)
    for (auto& t : threads) {
        t.join();
    }
    
    // Aggregate counts
    PopulationCounts total;
    for (const auto& r : results) {
        total += r;
    }
    
    grid_->swapGrids();
    return total;
}

PopulationCounts Simulation::runDayThreadPool(int day, ThreadPool& pool) {
    double P[9];
    precomputeP(P);
    
    std::vector<GridChunk> chunks = chunkStrategy_->divide(params_.N, params_.numChunks);
    
    // Submit tasks to pool
    std::vector<std::future<PopulationCounts>> futures;
    for (size_t i = 0; i < chunks.size(); ++i) {
        futures.push_back(pool.submit(
            [this, chunk = chunks[i], day, &P]() -> PopulationCounts {
                return grid_->updateChunk(
                    chunk, day, P, params_.mortality, params_.daysInfectious);
            }
        ));
    }
    
    // Wait for all tasks (via future::get - only coordination used)
    PopulationCounts total;
    for (auto& f : futures) {
        total += f.get();
    }
    
    grid_->swapGrids();
    return total;
}

SimSummary Simulation::run() {
    params_.print();
    
    SimSummary summary;
    
    // Setup CSV writer
    SimpleCSVWriter csv(params_.csvFile);
    csv.writeHeader({"day", "S", "I", "R", "V", "D", "beta_eff", "checksum"});
    
    // Setup snapshot writer if needed
    std::unique_ptr<SnapshotWriter> snapWriter;
    if (params_.snapshotInterval > 0) {
        snapWriter = std::make_unique<SnapshotWriter>(
            params_.snapshotDir, params_.maxPendingSnapshots, params_.ioLatencyMs);
    }
    
    // Create thread pool if needed
    std::unique_ptr<ThreadPool> pool;
    if (params_.mode == "thread_pool") {
        pool = std::make_unique<ThreadPool>(params_.numThreads);
    }
    
    // Write initial state
    PopulationCounts counts = grid_->countAll();
    uint64_t checksum = grid_->computeChecksum();
    csv.writeRow(0, counts.S, counts.I, counts.R, counts.V, counts.D,
                 betaEff_, checksum);
    
    // Track peak
    summary.peakInfectedFraction = counts.infectedFraction();
    summary.peakDay = 0;
    summary.totalInfected = counts.I;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Main simulation loop
    for (int day = 1; day <= params_.maxDays; ++day) {
        // Apply interventions based on previous day's counts
        double infFrac = counts.infectedFraction();
        betaEff_ = interventions_.computeBetaEff(params_.beta, infFrac);
        
        // Run one day
        if (params_.mode == "sequential") {
            counts = runDaySequential(day);
        } else if (params_.mode == "thread_per_day") {
            counts = runDayThreadPerDay(day);
        } else if (params_.mode == "thread_pool") {
            counts = runDayThreadPool(day, *pool);
        }
        
        checksum = grid_->computeChecksum();
        
        // Write CSV
        csv.writeRow(day, counts.S, counts.I, counts.R, counts.V, counts.D,
                     betaEff_, checksum);
        
        // Track peak
        if (counts.infectedFraction() > summary.peakInfectedFraction) {
            summary.peakInfectedFraction = counts.infectedFraction();
            summary.peakDay = day;
        }
        
        // Snapshot
        if (snapWriter && params_.snapshotInterval > 0 && day % params_.snapshotInterval == 0) {
            if (params_.asyncSnapshots) {
                snapWriter->writeAsync(grid_->getCurrentGrid(), params_.N, day);
            } else {
                snapWriter->writeBlocking(grid_->getCurrentGrid(), params_.N, day);
            }
        }
        
        // Check termination: no infected
        if (counts.I == 0) {
            std::cout << "Epidemic ended on day " << day << " (no infected remaining)" << std::endl;
            summary.numDays = day;
            break;
        }
        
        summary.numDays = day;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    summary.elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    // Wait for pending snapshots
    if (snapWriter) {
        auto snapResults = snapWriter->waitForAll();
        int failed = 0;
        for (const auto& r : snapResults) {
            if (!r.success) failed++;
        }
        if (failed > 0) {
            std::cerr << "WARNING: " << failed << " snapshot writes failed" << std::endl;
        }
        std::cout << "Snapshots: " << snapWriter->getTotalWritten() << " written, "
                  << snapWriter->getTotalFailed() << " failed" << std::endl;
    }
    
    // Compute final counts
    PopulationCounts finalCounts = grid_->countAll();
    summary.totalDeaths = finalCounts.D;
    summary.totalInfected = params_.N * params_.N - finalCounts.S - finalCounts.V;
    summary.finalChecksum = grid_->computeChecksum();
    
    // Print summary
    std::cout << "\n=== Simulation Summary ===" << std::endl;
    std::cout << "Days simulated:       " << summary.numDays << std::endl;
    std::cout << "Peak infected:        " << std::fixed << std::setprecision(4) 
              << summary.peakInfectedFraction * 100 << "% (day " << summary.peakDay << ")" << std::endl;
    std::cout << "Total infected:       " << summary.totalInfected << std::endl;
    std::cout << "Total deaths:         " << summary.totalDeaths << std::endl;
    std::cout << "Elapsed time:         " << std::fixed << std::setprecision(2) 
              << summary.elapsedMs << " ms" << std::endl;
    std::cout << "Final checksum:       " << summary.finalChecksum << std::endl;
    std::cout << "==========================" << std::endl;
    
    csv.flush();
    
    return summary;
}

const Grid& Simulation::getGrid() const { return *grid_; }
const SimParams& Simulation::getParams() const { return params_; }

// =================== Command Line Parsing ===================

SimParams parseArgs(int argc, char* argv[]) {
    SimParams params;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        auto getNextArg = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for argument: " + arg);
            }
            return argv[++i];
        };
        
        try {
            if (arg == "--N" || arg == "-N") {
                params.N = std::stoi(getNextArg());
            } else if (arg == "--beta" || arg == "-b") {
                params.beta = std::stod(getNextArg());
            } else if (arg == "--days-infectious" || arg == "-d") {
                params.daysInfectious = std::stoi(getNextArg());
            } else if (arg == "--mortality" || arg == "-mu") {
                params.mortality = std::stod(getNextArg());
            } else if (arg == "--initial-infected" || arg == "-I0") {
                params.initialInfected = std::stoi(getNextArg());
            } else if (arg == "--vaccine-fraction" || arg == "-v") {
                params.vaccineFraction = std::stod(getNextArg());
            } else if (arg == "--mask-factor" || arg == "-m") {
                params.maskFactor = std::stod(getNextArg());
            } else if (arg == "--lockdown-lambda") {
                params.lockdownLambda = std::stod(getNextArg());
            } else if (arg == "--lockdown-theta-on") {
                params.lockdownThetaOn = std::stod(getNextArg());
            } else if (arg == "--lockdown-theta-off") {
                params.lockdownThetaOff = std::stod(getNextArg());
            } else if (arg == "--seed" || arg == "-s") {
                params.seed = std::stoull(getNextArg());
            } else if (arg == "--max-days") {
                params.maxDays = std::stoi(getNextArg());
            } else if (arg == "--threads" || arg == "-T") {
                params.numThreads = std::stoi(getNextArg());
            } else if (arg == "--chunks" || arg == "-C") {
                params.numChunks = std::stoi(getNextArg());
            } else if (arg == "--chunk-strategy") {
                params.chunkStrategy = getNextArg();
            } else if (arg == "--mode") {
                params.mode = getNextArg();
            } else if (arg == "--csv") {
                params.csvFile = getNextArg();
            } else if (arg == "--snapshot-interval" || arg == "-K") {
                params.snapshotInterval = std::stoi(getNextArg());
            } else if (arg == "--snapshot-dir") {
                params.snapshotDir = getNextArg();
            } else if (arg == "--max-pending" || arg == "-Q") {
                params.maxPendingSnapshots = std::stoi(getNextArg());
            } else if (arg == "--io-latency") {
                params.ioLatencyMs = std::stoi(getNextArg());
            } else if (arg == "--async-snapshots") {
                params.asyncSnapshots = true;
            } else if (arg == "--help" || arg == "-h") {
                std::cout << "Usage: pandemic_sim [options]\n"
                    << "  --N <int>               Grid size (default: 1000)\n"
                    << "  --beta <float>          Transmission prob (default: 0.08)\n"
                    << "  --days-infectious <int> Days infectious (default: 7)\n"
                    << "  --mortality <float>     Mortality rate (default: 0.01)\n"
                    << "  --initial-infected <int> Initially infected (default: 10)\n"
                    << "  --vaccine-fraction <float> Vaccinated fraction (default: 0)\n"
                    << "  --mask-factor <float>   Mask factor (default: 1.0)\n"
                    << "  --lockdown-lambda <float> Lockdown factor (default: 0.3)\n"
                    << "  --lockdown-theta-on <float> Lockdown on threshold (default: 0.02)\n"
                    << "  --lockdown-theta-off <float> Lockdown off threshold (default: 0.005)\n"
                    << "  --seed <int>            Random seed (default: 42)\n"
                    << "  --max-days <int>        Maximum days (default: 365)\n"
                    << "  --mode <str>            sequential|thread_per_day|thread_pool\n"
                    << "  --threads <int>         Number of threads (default: 1)\n"
                    << "  --chunks <int>          Number of chunks (default: same as threads)\n"
                    << "  --chunk-strategy <str>  row_strip|square_block|interleaved\n"
                    << "  --csv <file>            Output CSV file (default: output.csv)\n"
                    << "  --snapshot-interval <int> Snapshot every K days (0=off)\n"
                    << "  --snapshot-dir <dir>    Snapshot directory (default: snapshots)\n"
                    << "  --async-snapshots       Use async snapshot writing\n"
                    << "  --max-pending <int>     Max pending async writes Q (default: 4)\n"
                    << "  --io-latency <int>      Simulated I/O latency ms (default: 0)\n"
                    << "  --help                  Show this help\n";
                exit(0);
            } else {
                throw std::invalid_argument("Unknown argument: " + arg);
            }
        } catch (const std::invalid_argument& e) {
            throw std::invalid_argument("Error parsing '" + arg + "': " + e.what());
        } catch (const std::out_of_range& e) {
            throw std::out_of_range("Value out of range for '" + arg + "': " + e.what());
        }
    }
    
    return params;
}

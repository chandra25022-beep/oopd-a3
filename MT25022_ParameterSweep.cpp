// MT25022 - J Chandra Harsha
// Parameter Sweep implementation

#include "MT25022_ParameterSweep.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

// =================== Static Run Function ===================

SweepResult ParameterSweep::executeRun(const SweepConfig& config, SimParams baseParams) {
    SweepResult result;
    result.runIndex = config.runIndex;
    result.beta = config.beta;
    result.vaccineFraction = config.vaccineFraction;
    result.seed = config.seed;
    result.completed = false;
    
    try {
        // Override params for this run
        baseParams.beta = config.beta;
        baseParams.vaccineFraction = config.vaccineFraction;
        baseParams.seed = config.seed;
        // Suppress per-day output for sweep runs
        baseParams.csvFile = "sweep_run_" + std::to_string(config.runIndex) + ".csv";
        
        Simulation sim(baseParams);
        result.summary = sim.run();
        result.completed = true;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.completed = false;
    }
    
    return result;
}

// =================== Constructor ===================

ParameterSweep::ParameterSweep(const SimParams& baseParams,
                               const std::vector<double>& betas,
                               const std::vector<double>& vacFractions,
                               const std::vector<uint64_t>& seeds,
                               const std::string& outputCsv,
                               int timeLimitSec)
    : baseParams_(baseParams), outputCsv_(outputCsv), timeLimitSeconds_(timeLimitSec) {
    
    int idx = 0;
    for (double b : betas) {
        for (double v : vacFractions) {
            for (uint64_t s : seeds) {
                configs_.emplace_back(b, v, s, idx++);
            }
        }
    }
    
    std::cout << "Parameter sweep configured: " << configs_.size() << " runs" << std::endl;
    std::cout << "  Beta values: ";
    for (double b : betas) std::cout << b << " ";
    std::cout << std::endl;
    std::cout << "  Vaccine fractions: ";
    for (double v : vacFractions) std::cout << v << " ";
    std::cout << std::endl;
    std::cout << "  Seeds: ";
    for (uint64_t s : seeds) std::cout << s << " ";
    std::cout << std::endl;
}

// =================== Run ===================

std::vector<SweepResult> ParameterSweep::run() {
    auto startTime = std::chrono::steady_clock::now();
    
    // Setup CSV writer
    ThreadSafeCSVWriter csv(outputCsv_);
    csv.writeHeader({"run_index", "beta", "vaccine_fraction", "seed",
                     "peak_infected_fraction", "peak_day", "total_infected",
                     "total_deaths", "num_days", "elapsed_ms", "completed"});
    
    // Submit all runs as async tasks
    // Each run returns a SweepResult via a future
    std::vector<std::future<SweepResult>> futures;
    for (const auto& config : configs_) {
        futures.push_back(std::async(std::launch::async, 
            executeRun, config, baseParams_));
    }
    
    // Collect results as they finish
    // We process in order here, but report completion order
    std::vector<SweepResult> results;
    int completedCount = 0;
    int totalRuns = static_cast<int>(futures.size());
    
    for (size_t i = 0; i < futures.size(); ++i) {
        // Check time limit
        if (timeLimitSeconds_ > 0) {
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            
            if (elapsedSec >= timeLimitSeconds_) {
                std::cout << "\nTime limit reached (" << timeLimitSeconds_ << "s). "
                          << "Waiting for running tasks..." << std::endl;
                
                // Wait for remaining futures with a short timeout
                for (size_t j = i; j < futures.size(); ++j) {
                    auto status = futures[j].wait_for(std::chrono::seconds(5));
                    if (status == std::future_status::ready) {
                        try {
                            SweepResult r = futures[j].get();
                            completedCount++;
                            std::cout << completedCount << "/" << totalRuns 
                                      << " runs done" << std::endl;
                            
                            csv.writeRow(r.runIndex, r.beta, r.vaccineFraction, r.seed,
                                        r.summary.peakInfectedFraction, r.summary.peakDay,
                                        r.summary.totalInfected, r.summary.totalDeaths,
                                        r.summary.numDays, r.summary.elapsedMs,
                                        r.completed ? 1 : 0);
                            results.push_back(r);
                        } catch (...) {
                            std::cerr << "Run " << j << " failed with exception" << std::endl;
                        }
                    } else {
                        std::cout << "Run " << j << " still running, reporting as incomplete" << std::endl;
                        SweepResult incomplete;
                        incomplete.runIndex = static_cast<int>(j);
                        incomplete.completed = false;
                        incomplete.error = "Time limit exceeded";
                        results.push_back(incomplete);
                    }
                }
                break;
            }
        }
        
        // Get result (waits for this specific future)
        try {
            SweepResult r = futures[i].get();
            completedCount++;
            
            std::cout << completedCount << "/" << totalRuns << " runs done";
            if (r.completed) {
                std::cout << " | beta=" << r.beta << " v=" << r.vaccineFraction
                          << " seed=" << r.seed << " peak=" 
                          << std::fixed << std::setprecision(4) 
                          << r.summary.peakInfectedFraction * 100 << "%";
            } else {
                std::cout << " | FAILED: " << r.error;
            }
            std::cout << std::endl;
            
            csv.writeRow(r.runIndex, r.beta, r.vaccineFraction, r.seed,
                        r.summary.peakInfectedFraction, r.summary.peakDay,
                        r.summary.totalInfected, r.summary.totalDeaths,
                        r.summary.numDays, r.summary.elapsedMs,
                        r.completed ? 1 : 0);
            
            results.push_back(r);
        } catch (const std::exception& e) {
            std::cerr << "Run " << i << " exception: " << e.what() << std::endl;
            SweepResult errResult;
            errResult.runIndex = static_cast<int>(i);
            errResult.completed = false;
            errResult.error = e.what();
            results.push_back(errResult);
        }
    }
    
    csv.flush();
    
    auto endTime = std::chrono::steady_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    std::cout << "\n=== Sweep Complete ===" << std::endl;
    std::cout << "Total runs: " << totalRuns << std::endl;
    std::cout << "Completed:  " << completedCount << std::endl;
    std::cout << "Total time: " << std::fixed << std::setprecision(2) << totalMs << " ms" << std::endl;
    std::cout << "Results:    " << outputCsv_ << std::endl;
    
    return results;
}

int ParameterSweep::totalRuns() const {
    return static_cast<int>(configs_.size());
}

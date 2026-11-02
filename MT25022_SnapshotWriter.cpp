// MT25022 - J Chandra Harsha
// Snapshot writer implementation

#include "MT25022_SnapshotWriter.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <thread>
#include <sstream>
#include <iomanip>

// =================== Color Mapping ===================

void SnapshotWriter::stateToColor(State s, unsigned char& r, unsigned char& g, unsigned char& b) {
    switch (s) {
        case State::S: r = 70;  g = 130; b = 230; break;  // Blue - Susceptible
        case State::I: r = 230; g = 50;  b = 50;  break;  // Red - Infected
        case State::R: r = 50;  g = 200; b = 80;  break;  // Green - Recovered
        case State::V: r = 240; g = 200; b = 40;  break;  // Yellow - Vaccinated
        case State::D: r = 80;  g = 80;  b = 80;  break;  // Gray - Dead
        default:       r = 255; g = 255; b = 255; break;
    }
}

// =================== Static Write Function ===================

SnapshotResult SnapshotWriter::writeSnapshot(
    const std::vector<std::vector<Person>>& gridCopy,
    int N, int day, const std::string& filename, int latencyMs) {
    
    SnapshotResult result;
    result.day = day;
    result.filename = filename;
    result.success = false;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        // Simulate I/O latency
        if (latencyMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(latencyMs));
        }
        
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }
        
        // Write PPM header (P6 binary format)
        file << "P6\n" << N << " " << N << "\n255\n";
        
        // Write pixel data
        std::vector<unsigned char> pixelRow(N * 3);
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                stateToColor(gridCopy[i][j].getState(), 
                            pixelRow[j * 3], pixelRow[j * 3 + 1], pixelRow[j * 3 + 2]);
            }
            file.write(reinterpret_cast<char*>(pixelRow.data()), pixelRow.size());
        }
        
        if (!file.good()) {
            throw std::runtime_error("Write error for file: " + filename);
        }
        
        file.close();
        result.success = true;
    } catch (const std::exception& e) {
        result.error = e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.writeTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    return result;
}

// =================== Constructor / Destructor ===================

SnapshotWriter::SnapshotWriter(const std::string& outputDir, int maxPending, int ioLatencyMs)
    : outputDir_(outputDir), maxPending_(maxPending), ioLatencyMs_(ioLatencyMs),
      totalWritten_(0), totalFailed_(0) {
    if (maxPending < 1) {
        throw std::invalid_argument("SnapshotWriter: maxPending must be >= 1");
    }
    
    // Create output directory
    try {
        std::filesystem::create_directories(outputDir);
    } catch (const std::exception& e) {
        throw std::runtime_error("SnapshotWriter: cannot create directory '" + 
                                 outputDir + "': " + e.what());
    }
}

SnapshotWriter::~SnapshotWriter() {
    try {
        waitForAll();
    } catch (...) {
        // Suppress exceptions in destructor
    }
}

// =================== Write Methods ===================

SnapshotResult SnapshotWriter::writeBlocking(
    const std::vector<std::vector<Person>>& grid, int N, int day) {
    
    std::ostringstream oss;
    oss << outputDir_ << "/snapshot_day_" << std::setw(5) << std::setfill('0') << day << ".ppm";
    std::string filename = oss.str();
    
    SnapshotResult result = writeSnapshot(grid, N, day, filename, ioLatencyMs_);
    
    if (result.success) {
        totalWritten_++;
    } else {
        totalFailed_++;
        std::cerr << "Snapshot write failed for day " << day << ": " << result.error << std::endl;
    }
    
    return result;
}

void SnapshotWriter::writeAsync(
    const std::vector<std::vector<Person>>& grid, int N, int day) {
    
    // If max pending reached, wait for the oldest one
    while (static_cast<int>(pending_.size()) >= maxPending_) {
        auto& oldest = pending_.front();
        try {
            SnapshotResult result = oldest.get();
            if (result.success) {
                totalWritten_++;
            } else {
                totalFailed_++;
                std::cerr << "Async snapshot failed for day " << result.day 
                          << ": " << result.error << std::endl;
            }
        } catch (const std::exception& e) {
            totalFailed_++;
            std::cerr << "Async snapshot exception: " << e.what() << std::endl;
        }
        pending_.pop();
    }
    
    std::ostringstream oss;
    oss << outputDir_ << "/snapshot_day_" << std::setw(5) << std::setfill('0') << day << ".ppm";
    std::string filename = oss.str();
    
    // IMPORTANT: We must copy the grid data because the simulation will
    // modify the grid while the async write is in progress.
    // Without this copy, we'd have a data race: the main thread would be
    // updating the grid for the next day while the async writer reads it.
    auto gridCopy = grid; // Deep copy
    
    auto future = std::async(std::launch::async, 
        writeSnapshot, std::move(gridCopy), N, day, filename, ioLatencyMs_);
    
    pending_.push(std::move(future));
}

std::vector<SnapshotResult> SnapshotWriter::waitForAll() {
    std::vector<SnapshotResult> results;
    
    while (!pending_.empty()) {
        auto& oldest = pending_.front();
        try {
            SnapshotResult result = oldest.get();
            if (result.success) {
                totalWritten_++;
            } else {
                totalFailed_++;
                std::cerr << "Pending snapshot failed for day " << result.day 
                          << ": " << result.error << std::endl;
            }
            results.push_back(result);
        } catch (const std::exception& e) {
            totalFailed_++;
            std::cerr << "Pending snapshot exception: " << e.what() << std::endl;
            SnapshotResult errResult;
            errResult.success = false;
            errResult.error = e.what();
            results.push_back(errResult);
        }
        pending_.pop();
    }
    
    return results;
}

int SnapshotWriter::getPendingCount() const { 
    return static_cast<int>(pending_.size()); 
}

int SnapshotWriter::getTotalWritten() const { return totalWritten_; }
int SnapshotWriter::getTotalFailed() const { return totalFailed_; }
void SnapshotWriter::setIOLatency(int ms) { ioLatencyMs_ = ms; }

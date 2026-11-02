// MT25022 - J Chandra Harsha
// Snapshot writer for saving grid state as PPM images (sync and async)

#ifndef MT25022_SNAPSHOTWRITER_H
#define MT25022_SNAPSHOTWRITER_H

#include "MT25022_Person.h"
#include <vector>
#include <string>
#include <future>
#include <queue>
#include <chrono>
#include <stdexcept>

/**
 * @brief Result of a snapshot write operation.
 */
struct SnapshotResult {
    int day;           ///< Day this snapshot represents
    bool success;      ///< Whether the write succeeded
    std::string error; ///< Error message if failed
    std::string filename; ///< Output filename
    double writeTimeMs;  ///< Time taken to write
};

/**
 * @brief Writes grid snapshots as PPM image files.
 * 
 * Supports both blocking (synchronous) and asynchronous writing.
 * In async mode, copies the grid and submits the write as a future,
 * allowing the simulation to continue.
 * 
 * Demonstrates RAII, async programming, and exception handling.
 */
class SnapshotWriter {
private:
    std::string outputDir_;                          ///< Output directory
    int maxPending_;                                 ///< Max pending async writes (Q)
    int ioLatencyMs_;                                ///< Simulated I/O latency
    std::queue<std::future<SnapshotResult>> pending_; ///< Pending async writes
    int totalWritten_;                               ///< Total snapshots written
    int totalFailed_;                                ///< Total failed writes
    
    /**
     * @brief Map a state to an RGB color.
     * S=blue, I=red, R=green, V=yellow, D=gray
     */
    static void stateToColor(State s, unsigned char& r, unsigned char& g, unsigned char& b);
    
    /**
     * @brief Write a grid snapshot to a PPM file.
     * This is the actual write function that may be called sync or async.
     * 
     * @param gridCopy Copy of the grid data
     * @param N        Grid dimension
     * @param day      Simulation day
     * @param filename Output filename
     * @param latencyMs Simulated I/O latency
     * @return SnapshotResult
     */
    static SnapshotResult writeSnapshot(
        const std::vector<std::vector<Person>>& gridCopy,
        int N, int day, const std::string& filename, int latencyMs);

public:
    /**
     * @brief Construct a snapshot writer.
     * @param outputDir   Directory for PPM files
     * @param maxPending  Maximum number of pending async writes (Q)
     * @param ioLatencyMs Simulated I/O latency in milliseconds
     * @throws std::invalid_argument if maxPending < 1
     */
    SnapshotWriter(const std::string& outputDir = "snapshots", 
                   int maxPending = 4, 
                   int ioLatencyMs = 0);
    
    /** @brief Destructor: waits for all pending writes. */
    ~SnapshotWriter();
    
    // Non-copyable
    SnapshotWriter(const SnapshotWriter&) = delete;
    SnapshotWriter& operator=(const SnapshotWriter&) = delete;
    
    /**
     * @brief Write a snapshot synchronously (blocking).
     * @param grid Current grid data
     * @param N    Grid dimension
     * @param day  Current day
     * @return SnapshotResult
     */
    SnapshotResult writeBlocking(
        const std::vector<std::vector<Person>>& grid, int N, int day);
    
    /**
     * @brief Submit a snapshot for asynchronous writing.
     * 
     * Makes a deep copy of the grid (necessary because the simulation
     * will modify the grid while the write is in progress), then submits
     * the write as an async task.
     * 
     * If maxPending_ async writes are already pending, waits for the oldest
     * one to finish before submitting.
     * 
     * @param grid Current grid data
     * @param N    Grid dimension
     * @param day  Current day
     */
    void writeAsync(
        const std::vector<std::vector<Person>>& grid, int N, int day);
    
    /**
     * @brief Wait for all pending async writes to finish.
     * Reports any failures.
     * @return Vector of results from all pending writes
     */
    std::vector<SnapshotResult> waitForAll();
    
    /** @brief Get count of pending writes. */
    int getPendingCount() const;
    
    /** @brief Get total snapshots written. */
    int getTotalWritten() const;
    
    /** @brief Get total failed writes. */
    int getTotalFailed() const;
    
    /** @brief Set the I/O latency. */
    void setIOLatency(int ms);
};

#endif // MT25022_SNAPSHOTWRITER_H

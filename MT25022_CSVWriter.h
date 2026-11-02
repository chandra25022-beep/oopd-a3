// MT25022 - J Chandra Harsha
// CSV Writer utility - template-based for flexibility

#ifndef MT25022_CSVWRITER_H
#define MT25022_CSVWRITER_H

#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <mutex>

/**
 * @brief Thread-safe CSV file writer.
 * 
 * Uses templates to support different data types in rows.
 * Demonstrates RAII for file handle management.
 * 
 * @tparam MutexType Type of mutex for thread safety (use std::mutex or a no-op)
 */
template <typename MutexType = std::mutex>
class CSVWriter {
private:
    std::ofstream file_;
    std::string filename_;
    MutexType mutex_;
    bool headerWritten_;
    
public:
    /**
     * @brief Open a CSV file for writing.
     * @param filename Path to the CSV file
     * @throws std::runtime_error if file cannot be opened
     */
    explicit CSVWriter(const std::string& filename) 
        : filename_(filename), headerWritten_(false) {
        file_.open(filename);
        if (!file_.is_open()) {
            throw std::runtime_error("CSVWriter: cannot open file '" + filename + "' for writing");
        }
    }
    
    /** @brief Destructor flushes and closes the file (RAII). */
    ~CSVWriter() {
        if (file_.is_open()) {
            file_.flush();
            file_.close();
        }
    }
    
    // Non-copyable
    CSVWriter(const CSVWriter&) = delete;
    CSVWriter& operator=(const CSVWriter&) = delete;
    
    // Movable
    CSVWriter(CSVWriter&& other) noexcept 
        : file_(std::move(other.file_)), filename_(std::move(other.filename_)),
          headerWritten_(other.headerWritten_) {}
    
    /**
     * @brief Write header row.
     * @param headers Column names
     */
    void writeHeader(const std::vector<std::string>& headers) {
        std::lock_guard<MutexType> lock(mutex_);
        for (size_t i = 0; i < headers.size(); ++i) {
            if (i > 0) file_ << ",";
            file_ << headers[i];
        }
        file_ << "\n";
        headerWritten_ = true;
    }
    
    /**
     * @brief Write a data row using variadic template.
     * Demonstrates template parameter packing.
     */
    template <typename... Args>
    void writeRow(Args&&... args) {
        std::lock_guard<MutexType> lock(mutex_);
        std::ostringstream oss;
        writeFields(oss, std::forward<Args>(args)...);
        file_ << oss.str() << "\n";
    }
    
    /**
     * @brief Write a row from a vector of strings.
     */
    void writeRowVec(const std::vector<std::string>& fields) {
        std::lock_guard<MutexType> lock(mutex_);
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) file_ << ",";
            file_ << fields[i];
        }
        file_ << "\n";
    }
    
    /** @brief Flush the file to disk. */
    void flush() {
        std::lock_guard<MutexType> lock(mutex_);
        file_.flush();
    }
    
    /** @brief Get the filename. */
    const std::string& getFilename() const { return filename_; }
    
private:
    // Base case: single field
    template <typename T>
    void writeFields(std::ostringstream& oss, T&& val) {
        oss << std::forward<T>(val);
    }
    
    // Recursive case: first field, then rest
    template <typename T, typename... Rest>
    void writeFields(std::ostringstream& oss, T&& val, Rest&&... rest) {
        oss << std::forward<T>(val) << ",";
        writeFields(oss, std::forward<Rest>(rest)...);
    }
};

/**
 * @brief No-op mutex for single-threaded use.
 */
struct NoMutex {
    void lock() {}
    void unlock() {}
};

// Type aliases for convenience
using ThreadSafeCSVWriter = CSVWriter<std::mutex>;
using SimpleCSVWriter = CSVWriter<NoMutex>;

#endif // MT25022_CSVWRITER_H

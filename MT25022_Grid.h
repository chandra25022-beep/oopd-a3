// MT25022 - J Chandra Harsha
// Grid class representing the N×N simulation grid with double buffering

#ifndef MT25022_GRID_H
#define MT25022_GRID_H

#include "MT25022_Person.h"
#include "MT25022_HashRNG.h"
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>
#include <stdexcept>
#include <random>

/**
 * @brief Struct holding daily population counts.
 */
struct PopulationCounts {
    int S = 0;   ///< Susceptible count
    int I = 0;   ///< Infected count
    int R = 0;   ///< Recovered count
    int V = 0;   ///< Vaccinated count
    int D = 0;   ///< Dead count
    
    /** @brief Aggregate counts from another struct. */
    PopulationCounts& operator+=(const PopulationCounts& other) {
        S += other.S; I += other.I; R += other.R;
        V += other.V; D += other.D;
        return *this;
    }
    
    /** @brief Total population. */
    int total() const { return S + I + R + V + D; }
    
    /** @brief Infected fraction. */
    double infectedFraction() const {
        int t = total();
        return (t > 0) ? static_cast<double>(I) / t : 0.0;
    }
};

/**
 * @brief Defines a rectangular chunk of the grid for parallel processing.
 */
struct GridChunk {
    int startRow;  ///< First row (inclusive)
    int endRow;    ///< Last row (exclusive)
    int startCol;  ///< First column (inclusive)
    int endCol;    ///< Last column (exclusive)
    
    GridChunk(int sr, int er, int sc, int ec)
        : startRow(sr), endRow(er), startCol(sc), endCol(ec) {}
};

/**
 * @brief Abstract base class for grid chunking strategies.
 * Demonstrates polymorphism: different strategies for dividing the grid.
 */
class ChunkStrategy {
public:
    virtual ~ChunkStrategy() = default;
    
    /**
     * @brief Divide the grid into chunks.
     * @param N     Grid dimension
     * @param numChunks Number of chunks to create
     * @return Vector of GridChunk describing each chunk
     */
    virtual std::vector<GridChunk> divide(int N, int numChunks) const = 0;
    
    /** @brief Get the name of this strategy. */
    virtual std::string getName() const = 0;
};

/**
 * @brief Divide by contiguous strips of rows.
 */
class RowStripStrategy : public ChunkStrategy {
public:
    std::vector<GridChunk> divide(int N, int numChunks) const override;
    std::string getName() const override { return "row_strip"; }
};

/**
 * @brief Divide into approximately square blocks.
 */
class SquareBlockStrategy : public ChunkStrategy {
public:
    std::vector<GridChunk> divide(int N, int numChunks) const override;
    std::string getName() const override { return "square_block"; }
};

/**
 * @brief Interleaved rows: thread i takes rows i, i+T, i+2T, ...
 */
class InterleavedRowStrategy : public ChunkStrategy {
public:
    std::vector<GridChunk> divide(int N, int numChunks) const override;
    std::string getName() const override { return "interleaved"; }
};

/**
 * @brief Factory function to create a chunk strategy by name.
 * @param name Strategy name: "row_strip", "square_block", or "interleaved"
 * @return unique_ptr to the strategy
 * @throws std::invalid_argument if name is unknown
 */
std::unique_ptr<ChunkStrategy> createChunkStrategy(const std::string& name);

/**
 * @brief The simulation grid with double buffering.
 * 
 * Maintains two N×N grids (current and next). During a simulation step,
 * threads read from current and write to next. After all threads complete,
 * the grids are swapped.
 * 
 * Demonstrates composition (has-a Person), RAII, operator overloading.
 */
class Grid {
private:
    int N_;                                ///< Grid dimension
    std::vector<std::vector<Person>> current_;  ///< Current day's state
    std::vector<std::vector<Person>> next_;     ///< Next day's state being computed
    uint64_t seed_;                         ///< Random seed

public:
    // =================== Constructors ===================
    
    /**
     * @brief Construct an N×N grid.
     * @param N    Grid dimension (must be > 0)
     * @param seed Random seed for initialization
     * @throws std::invalid_argument if N <= 0
     */
    Grid(int N, uint64_t seed);
    
    /** @brief Copy constructor (deep copy). */
    Grid(const Grid& other);
    
    /** @brief Move constructor. */
    Grid(Grid&& other) noexcept;
    
    /** @brief Copy assignment. */
    Grid& operator=(const Grid& other);
    
    /** @brief Move assignment. */
    Grid& operator=(Grid&& other) noexcept;
    
    ~Grid() = default;

    // =================== Grid Access ===================
    
    /** @brief Access current grid cell (read-only). */
    const Person& operator()(int i, int j) const;
    
    /** @brief Access next grid cell (write). */
    Person& nextCell(int i, int j);
    
    /** @brief Get the current grid cell. */
    const Person& getCurrent(int i, int j) const;
    
    /** @brief Get grid dimension. */
    int getN() const;
    
    /** @brief Get the seed. */
    uint64_t getSeed() const;

    // =================== Initialization ===================
    
    /**
     * @brief Initialize the grid with vaccinations and infections.
     * @param vaccineFraction Fraction of population to vaccinate
     * @param initialInfected Number of initially infected people
     * @throws std::invalid_argument if parameters are invalid
     */
    void initialize(double vaccineFraction, int initialInfected);

    // =================== Simulation ===================
    
    /**
     * @brief Swap current and next grids (after all threads complete).
     * Uses move semantics for efficiency.
     */
    void swapGrids();
    
    /**
     * @brief Update a chunk of the grid for one day.
     * Reads from current, writes to next.
     * @param chunk     The chunk to update
     * @param day       Current simulation day
     * @param P         Precomputed infection probabilities P[0..8]
     * @param mu        Mortality probability
     * @param d         Days infectious
     * @return PopulationCounts for this chunk
     */
    PopulationCounts updateChunk(const GridChunk& chunk, int day, 
                                  const double P[9], double mu, int d);
    
    /**
     * @brief Count the number of infected neighbours of cell (i, j).
     * @param i Row
     * @param j Column
     * @return Number of infected neighbours (0-8)
     */
    int countInfectedNeighbours(int i, int j) const;
    
    /**
     * @brief Count all states in the current grid.
     * @return PopulationCounts
     */
    PopulationCounts countAll() const;
    
    /**
     * @brief Count states in a specific chunk.
     */
    PopulationCounts countChunk(const GridChunk& chunk) const;
    
    /**
     * @brief Compute a hash checksum of the entire current grid.
     * @return uint64_t checksum
     */
    uint64_t computeChecksum() const;
    
    /**
     * @brief Create a deep copy of the current grid data.
     * Used for async snapshot writing.
     * @return A copy of the current grid
     */
    std::vector<std::vector<Person>> copyCurrentGrid() const;
    
    /**
     * @brief Get a const reference to the current grid data.
     */
    const std::vector<std::vector<Person>>& getCurrentGrid() const;
};

#endif // MT25022_GRID_H

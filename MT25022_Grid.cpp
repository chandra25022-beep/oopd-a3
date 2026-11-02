// MT25022 - J Chandra Harsha
// Grid class implementation

#include "MT25022_Grid.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>

// =================== ChunkStrategy Implementations ===================

std::vector<GridChunk> RowStripStrategy::divide(int N, int numChunks) const {
    std::vector<GridChunk> chunks;
    int rowsPerChunk = N / numChunks;
    int remainder = N % numChunks;
    int startRow = 0;
    
    for (int i = 0; i < numChunks; ++i) {
        int rows = rowsPerChunk + (i < remainder ? 1 : 0);
        if (rows > 0) {
            chunks.emplace_back(startRow, startRow + rows, 0, N);
        }
        startRow += rows;
    }
    return chunks;
}

std::vector<GridChunk> SquareBlockStrategy::divide(int N, int numChunks) const {
    // Find best grid of blocks: try to make blocks as square as possible
    int bestRowDiv = 1, bestColDiv = numChunks;
    double bestRatio = 1e18;
    
    for (int r = 1; r <= numChunks; ++r) {
        if (numChunks % r == 0) {
            int c = numChunks / r;
            double rowSize = static_cast<double>(N) / r;
            double colSize = static_cast<double>(N) / c;
            double ratio = std::max(rowSize, colSize) / std::max(1.0, std::min(rowSize, colSize));
            if (ratio < bestRatio) {
                bestRatio = ratio;
                bestRowDiv = r;
                bestColDiv = c;
            }
        }
    }
    
    std::vector<GridChunk> chunks;
    for (int ri = 0; ri < bestRowDiv; ++ri) {
        int sr = ri * N / bestRowDiv;
        int er = (ri + 1) * N / bestRowDiv;
        for (int ci = 0; ci < bestColDiv; ++ci) {
            int sc = ci * N / bestColDiv;
            int ec = (ci + 1) * N / bestColDiv;
            if (er > sr && ec > sc) {
                chunks.emplace_back(sr, er, sc, ec);
            }
        }
    }
    return chunks;
}

std::vector<GridChunk> InterleavedRowStrategy::divide(int N, int numChunks) const {
    // Each chunk gets interleaved rows: chunk i gets rows i, i+numChunks, i+2*numChunks, ...
    // We represent each "chunk" as multiple single-row chunks for the same thread
    // But since we need contiguous chunks for simplicity, we'll return per-row chunks
    // grouped by thread. The caller must handle accumulation per thread.
    
    // For simplicity: return one chunk per row, with chunk index = row % numChunks
    // Actually, we'll return numChunks chunks, each being a list of rows.
    // Since GridChunk is rectangular, we'll use a different approach:
    // Return individual row chunks and let the caller group them.
    
    std::vector<GridChunk> chunks;
    for (int t = 0; t < numChunks; ++t) {
        for (int row = t; row < N; row += numChunks) {
            chunks.emplace_back(row, row + 1, 0, N);
        }
    }
    // We need to tag which thread each chunk belongs to
    // Since the caller processes all chunks anyway, each chunk is a single row
    // and the threading layer groups them by thread index
    return chunks;
}

std::unique_ptr<ChunkStrategy> createChunkStrategy(const std::string& name) {
    if (name == "row_strip") {
        return std::make_unique<RowStripStrategy>();
    } else if (name == "square_block") {
        return std::make_unique<SquareBlockStrategy>();
    } else if (name == "interleaved") {
        return std::make_unique<InterleavedRowStrategy>();
    } else {
        throw std::invalid_argument("Unknown chunk strategy: " + name + 
                                    ". Options: row_strip, square_block, interleaved");
    }
}

// =================== Grid Implementation ===================

Grid::Grid(int N, uint64_t seed) : N_(N), seed_(seed) {
    if (N <= 0) {
        throw std::invalid_argument("Grid: dimension N must be positive (got " + std::to_string(N) + ")");
    }
    current_.resize(N, std::vector<Person>(N));
    next_.resize(N, std::vector<Person>(N));
}

Grid::Grid(const Grid& other) 
    : N_(other.N_), current_(other.current_), next_(other.next_), seed_(other.seed_) {}

Grid::Grid(Grid&& other) noexcept
    : N_(other.N_), current_(std::move(other.current_)), 
      next_(std::move(other.next_)), seed_(other.seed_) {
    other.N_ = 0;
}

Grid& Grid::operator=(const Grid& other) {
    if (this != &other) {
        N_ = other.N_;
        current_ = other.current_;
        next_ = other.next_;
        seed_ = other.seed_;
    }
    return *this;
}

Grid& Grid::operator=(Grid&& other) noexcept {
    if (this != &other) {
        N_ = other.N_;
        current_ = std::move(other.current_);
        next_ = std::move(other.next_);
        seed_ = other.seed_;
        other.N_ = 0;
    }
    return *this;
}

// =================== Grid Access ===================

const Person& Grid::operator()(int i, int j) const {
    if (i < 0 || i >= N_ || j < 0 || j >= N_) {
        throw std::out_of_range("Grid::operator(): index (" + std::to_string(i) + 
                                ", " + std::to_string(j) + ") out of range [0, " + 
                                std::to_string(N_) + ")");
    }
    return current_[i][j];
}

Person& Grid::nextCell(int i, int j) {
    return next_[i][j];
}

const Person& Grid::getCurrent(int i, int j) const {
    return current_[i][j];
}

int Grid::getN() const { return N_; }
uint64_t Grid::getSeed() const { return seed_; }

// =================== Initialization ===================

void Grid::initialize(double vaccineFraction, int initialInfected) {
    if (vaccineFraction < 0.0 || vaccineFraction > 1.0) {
        throw std::invalid_argument("Grid::initialize: vaccine fraction must be in [0, 1], got " + 
                                    std::to_string(vaccineFraction));
    }
    if (initialInfected < 0 || initialInfected > N_ * N_) {
        throw std::invalid_argument("Grid::initialize: initial infected must be in [0, N*N]");
    }
    
    // Reset all to susceptible
    for (int i = 0; i < N_; ++i) {
        for (int j = 0; j < N_; ++j) {
            current_[i][j] = Person(State::S);
        }
    }
    
    // Step 1: Vaccinate a fraction v of the population at random
    // Use a deterministic approach: generate a random value for each cell
    // and vaccinate if < vaccineFraction
    if (vaccineFraction > 0.0) {
        for (int i = 0; i < N_; ++i) {
            for (int j = 0; j < N_; ++j) {
                int cellId = i * N_ + j;
                double r = HashRNG<double>::randInit(seed_, cellId, 0);
                if (r < vaccineFraction) {
                    current_[i][j].setState(State::V);
                }
            }
        }
    }
    
    // Step 2: Infect I0 susceptible people at random
    // Collect all susceptible cells, then select I0 using Fisher-Yates with deterministic RNG
    std::vector<int> susceptibleCells;
    for (int i = 0; i < N_; ++i) {
        for (int j = 0; j < N_; ++j) {
            if (current_[i][j].isSusceptible()) {
                susceptibleCells.push_back(i * N_ + j);
            }
        }
    }
    
    if (initialInfected > static_cast<int>(susceptibleCells.size())) {
        throw std::invalid_argument("Grid::initialize: not enough susceptible cells for initial infections");
    }
    
    // Deterministic selection using seeded shuffle
    std::mt19937_64 initRng(seed_);
    std::shuffle(susceptibleCells.begin(), susceptibleCells.end(), initRng);
    
    for (int k = 0; k < initialInfected; ++k) {
        int cellId = susceptibleCells[k];
        int i = cellId / N_;
        int j = cellId % N_;
        current_[i][j].setState(State::I);
        current_[i][j].setCounter(0);
    }
    
    // Copy current to next as initial state
    next_ = current_;
}

// =================== Simulation ===================

void Grid::swapGrids() {
    std::swap(current_, next_);
}

int Grid::countInfectedNeighbours(int i, int j) const {
    int count = 0;
    for (int di = -1; di <= 1; ++di) {
        for (int dj = -1; dj <= 1; ++dj) {
            if (di == 0 && dj == 0) continue;
            int ni = i + di, nj = j + dj;
            if (ni >= 0 && ni < N_ && nj >= 0 && nj < N_) {
                if (current_[ni][nj].isInfected()) {
                    ++count;
                }
            }
        }
    }
    return count;
}

PopulationCounts Grid::updateChunk(const GridChunk& chunk, int day, 
                                    const double P[9], double mu, int d) {
    PopulationCounts counts;
    
    for (int i = chunk.startRow; i < chunk.endRow; ++i) {
        for (int j = chunk.startCol; j < chunk.endCol; ++j) {
            int cellId = i * N_ + j;
            const Person& cur = current_[i][j];
            
            if (cur.isSusceptible()) {
                // Infection rule
                int k = countInfectedNeighbours(i, j);
                if (k > 0 && HashRNG<double>::rand01(seed_, day, cellId, 0) < P[k]) {
                    next_[i][j] = Person(State::I, 0);
                    counts.I++;
                } else {
                    next_[i][j] = Person(State::S);
                    counts.S++;
                }
            } else if (cur.isInfected()) {
                int c = cur.getCounter();
                if (c + 1 == d) {
                    // End of infection: die or recover
                    if (HashRNG<double>::rand01(seed_, day, cellId, 1) < mu) {
                        next_[i][j] = Person(State::D);
                        counts.D++;
                    } else {
                        next_[i][j] = Person(State::R);
                        counts.R++;
                    }
                } else {
                    // Continue being infected
                    next_[i][j] = Person(State::I, c + 1);
                    counts.I++;
                }
            } else {
                // R, V, D: no change
                next_[i][j] = cur;
                switch (cur.getState()) {
                    case State::R: counts.R++; break;
                    case State::V: counts.V++; break;
                    case State::D: counts.D++; break;
                    default: break;
                }
            }
        }
    }
    
    return counts;
}

PopulationCounts Grid::countAll() const {
    PopulationCounts counts;
    for (int i = 0; i < N_; ++i) {
        for (int j = 0; j < N_; ++j) {
            switch (current_[i][j].getState()) {
                case State::S: counts.S++; break;
                case State::I: counts.I++; break;
                case State::R: counts.R++; break;
                case State::V: counts.V++; break;
                case State::D: counts.D++; break;
            }
        }
    }
    return counts;
}

PopulationCounts Grid::countChunk(const GridChunk& chunk) const {
    PopulationCounts counts;
    for (int i = chunk.startRow; i < chunk.endRow; ++i) {
        for (int j = chunk.startCol; j < chunk.endCol; ++j) {
            switch (current_[i][j].getState()) {
                case State::S: counts.S++; break;
                case State::I: counts.I++; break;
                case State::R: counts.R++; break;
                case State::V: counts.V++; break;
                case State::D: counts.D++; break;
            }
        }
    }
    return counts;
}

uint64_t Grid::computeChecksum() const {
    uint64_t checksum = 0;
    for (int i = 0; i < N_; ++i) {
        for (int j = 0; j < N_; ++j) {
            checksum ^= current_[i][j].hashValue() * (i * N_ + j + 1);
        }
    }
    return checksum;
}

std::vector<std::vector<Person>> Grid::copyCurrentGrid() const {
    return current_;
}

const std::vector<std::vector<Person>>& Grid::getCurrentGrid() const {
    return current_;
}

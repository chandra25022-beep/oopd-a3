// MT25022 - J Chandra Harsha
// Deterministic hash-based random number generator
// Produces identical results regardless of thread count

#ifndef MT25022_HASHRNG_H
#define MT25022_HASHRNG_H

#include <cstdint>
#include <cstddef>
#include <stdexcept>

/**
 * @brief Deterministic hash-based RNG using SplitMix64.
 * 
 * Every random number depends only on (seed, day, cellId, stream),
 * ensuring identical results regardless of thread count or grid division.
 * 
 * @tparam T The floating-point return type (float or double)
 */
template <typename T = double>
class HashRNG {
public:
    /**
     * @brief Generate a deterministic random number in [0, 1).
     * 
     * @param seed      Global simulation seed
     * @param day       Current simulation day
     * @param cellId    Cell identifier (i * N + j)
     * @param stream    Decision stream (0 = infection, 1 = death)
     * @return T        Random value in [0, 1)
     * @throws std::invalid_argument if stream is not 0 or 1
     */
    static T rand01(uint64_t seed, int day, int cellId, int stream) {
        if (stream < 0 || stream > 1) {
            throw std::invalid_argument("HashRNG: stream must be 0 (infection) or 1 (death)");
        }
        // Combine all inputs into a single 64-bit state
        uint64_t state = seed;
        state = splitmix64(state + static_cast<uint64_t>(day));
        state = splitmix64(state + static_cast<uint64_t>(cellId));
        state = splitmix64(state + static_cast<uint64_t>(stream));
        
        // Convert to [0, 1)
        return static_cast<T>(state >> 11) * static_cast<T>(1.0 / (1ULL << 53));
    }

    /**
     * @brief Generate a deterministic random number for initialization.
     * Used during vaccination and initial infection setup.
     * 
     * @param seed      Global simulation seed
     * @param cellId    Cell identifier
     * @param purpose   0 = vaccination, 1 = initial infection
     * @return T        Random value in [0, 1)
     */
    static T randInit(uint64_t seed, int cellId, int purpose) {
        uint64_t state = seed;
        state = splitmix64(state + static_cast<uint64_t>(cellId));
        state = splitmix64(state + static_cast<uint64_t>(purpose) + 1000ULL);
        return static_cast<T>(state >> 11) * static_cast<T>(1.0 / (1ULL << 53));
    }

private:
    /**
     * @brief SplitMix64 hash function - fast, high quality mixing.
     */
    static uint64_t splitmix64(uint64_t x) {
        x += 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    }
};

#endif // MT25022_HASHRNG_H

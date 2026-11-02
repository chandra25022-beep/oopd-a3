# Pandemic Spread Simulation
## OOPD Assignment 3 - MT25022 (J Chandra Harsha)

### Overview
A simulation of infectious disease spreading through an N×N grid population, featuring:
- **Part A**: Sequential single-threaded simulation
- **Part B**: Parallel execution (thread-per-day and thread pool)
- **Part C**: Asynchronous snapshot writing and parameter sweep
- **Part D**: Performance study (strong/weak scaling, chunk size optimization)
- **Part E**: Epidemiological analysis

### Building (CMake – Cross-Platform)

Works on **Windows**, **Linux**, and **macOS**.

```bash
# 1. Create build directory and configure
cmake -S . -B build

# 2. Build all executables
cmake --build build

# 3. (Optional) Build in Release mode for performance experiments
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

**Windows (Visual Studio)**:
```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
# Executables will be in build\Release\
```

**Windows (MinGW)**:
```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

**Linux / macOS (GCC / Clang)**:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

#### Fallback: GNU Make (Linux / macOS only)
```bash
make all
```

### Running

#### Sequential Simulation (Part A)
```bash
./pandemic_sim --mode sequential --N 1000 --max-days 365 --seed 42 --csv output.csv
```

#### Threaded Simulation (Part B)
```bash
# Thread-per-day with row strips
./pandemic_sim --mode thread_per_day --threads 4 --chunks 4 --chunk-strategy row_strip

# Thread pool with square blocks
./pandemic_sim --mode thread_pool --threads 4 --chunks 16 --chunk-strategy square_block

# Interleaved rows
./pandemic_sim --mode thread_pool --threads 8 --chunk-strategy interleaved
```

#### Correctness Test (Part B)
```bash
make test
# Or manually:
./test_correctness --N 100 --max-days 50
```

#### Snapshots (Part C1)
```bash
# Blocking snapshots
./pandemic_sim --snapshot-interval 10 --N 500 --max-days 200

# Async snapshots
./pandemic_sim --snapshot-interval 10 --async-snapshots --max-pending 4

# With simulated I/O latency
./pandemic_sim --snapshot-interval 5 --async-snapshots --io-latency 100
```

#### Parameter Sweep (Part C2)
```bash
./sweep --N 500 --max-days 200 \
  --sweep-betas 0.04,0.06,0.08,0.10,0.12 \
  --sweep-vaccines 0.0,0.1,0.2,0.3,0.5 \
  --sweep-seeds 42,43,44,45,46 \
  --sweep-output sweep_results.csv \
  --sweep-time-limit 300
```

### All Command-Line Options

| Option | Short | Default | Description |
|--------|-------|---------|-------------|
| `--N` | `-N` | 1000 | Grid dimension |
| `--beta` | `-b` | 0.08 | Transmission probability |
| `--days-infectious` | `-d` | 7 | Days infectious |
| `--mortality` | `-mu` | 0.01 | Mortality probability |
| `--initial-infected` | `-I0` | 10 | Initially infected |
| `--vaccine-fraction` | `-v` | 0.0 | Vaccinated fraction |
| `--mask-factor` | `-m` | 1.0 | Mask factor (1=off) |
| `--lockdown-lambda` | | 0.3 | Lockdown factor |
| `--lockdown-theta-on` | | 0.02 | Lockdown trigger |
| `--lockdown-theta-off` | | 0.005 | Lockdown lift |
| `--seed` | `-s` | 42 | Random seed |
| `--max-days` | | 365 | Maximum days |
| `--mode` | | sequential | sequential/thread_per_day/thread_pool |
| `--threads` | `-T` | 1 | Number of threads |
| `--chunks` | `-C` | =threads | Number of chunks |
| `--chunk-strategy` | | row_strip | row_strip/square_block/interleaved |
| `--csv` | | output.csv | Output CSV path |
| `--snapshot-interval` | `-K` | 0 | Snapshot every K days |
| `--async-snapshots` | | false | Use async writing |
| `--max-pending` | `-Q` | 4 | Max pending writes |
| `--io-latency` | | 0 | Simulated I/O latency (ms) |

### Design: No Data Races

The double-buffering design ensures no data races:
1. **Two grids**: `current` (read-only during a day) and `next` (write-only).
2. **Each thread writes only its own cells** in `next`. No two threads write the same cell.
3. **All threads read from `current`**, which is not modified until after all threads join.
4. **Grid swap happens only after all threads are joined**, ensuring a happens-before relationship.

No locks, atomics, or barriers are needed. Only `thread::join` and `future::get` are used for coordination.

### Async Snapshots: Why Copy?

The grid must be deep-copied before async writing because:
- The main thread continues simulating (modifying the grid) while the write is in progress.
- Without a copy, the writer thread would read the grid while the simulation thread modifies it, causing a data race.

### OOP Concepts Used

| Concept | Implementation |
|---------|---------------|
| **Inheritance** | `ChunkStrategy` → `RowStripStrategy`, `SquareBlockStrategy`, `InterleavedRowStrategy` |
| **Polymorphism** | Virtual `divide()` method, `Intervention` hierarchy |
| **Composition** | `Simulation` contains `Grid`, `InterventionManager`, `SnapshotWriter` |
| **Templates** | `HashRNG<T>`, `CSVWriter<MutexType>`, `ThreadPool::submit<F, Args...>` |
| **Exception Handling** | All parameter validation, file I/O, thread operations |
| **RAII** | `ThreadPool` destructor joins threads, `CSVWriter` closes files |
| **Encapsulation** | Private members with validated public accessors |
| **Operator Overloading** | `Grid::operator()`, `Person::operator==`, `PopulationCounts::operator+=` |
| **Move Semantics** | Grid swap, future management |
| **Smart Pointers** | `unique_ptr<Grid>`, `unique_ptr<ChunkStrategy>`, `unique_ptr<ThreadPool>` |
| **Lambda Functions** | Thread tasks, chunk processing |
| **Enum Class** | `State { S, I, R, V, D }` |

### File Structure

```
MT25022_HashRNG.h          - Deterministic hash-based RNG (template)
MT25022_Person.h/.cpp      - Person class (state + counter)
MT25022_Grid.h/.cpp        - Grid class with double buffering + chunk strategies
MT25022_Intervention.h/.cpp - Mask and lockdown interventions (inheritance)
MT25022_ThreadPool.h/.cpp  - Reusable thread pool
MT25022_CSVWriter.h        - Template CSV writer with RAII
MT25022_SnapshotWriter.h/.cpp - PPM snapshot writer (sync + async)
MT25022_Simulation.h/.cpp  - Main simulation engine
MT25022_ParameterSweep.h/.cpp - Parameter sweep runner
MT25022_Main.cpp           - Main entry point
MT25022_Sweep.cpp          - Sweep entry point
MT25022_Test.cpp           - Correctness verification
Makefile                   - Build system
Readme.md                  - This file
```

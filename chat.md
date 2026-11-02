# OOPD Assignment 3 – Complete Development Chat & Session Log
## Course: CSE600 – Object Oriented Programming & Design (2026)
### Student Details:
- **Roll Number:** `MT25022`
- **Student Name:** `J Chandra Harsha`
- **Date:** November 2, 2026
- **Project:** Parallel and Asynchronous Simulation of Pandemic Spread

---

## 1. Executive Summary & Purpose

This document serves as the complete, portable brain and handover log for **Assignment 3**. If you transfer this folder to another machine (Windows laptop/desktop, Linux workstation, or macOS), this document provides everything you need to know:
1. What was requested and all strict assignment rules.
2. The entire software architecture and design decisions.
3. Every file created and its role.
4. How the code satisfies all OOP concepts (Inheritance, Polymorphism, Templates, RAII, Exception Handling, Operator Overloading, etc.).
5. How to build and run using **CMake** on any operating system.
6. Verification tests completed (all **54/54 tests passed**).
7. Detailed next steps to complete the experimental report (Parts D & E) and final zip submission.

---

## 2. Strict Submission Guidelines & Compliance

| Rule | Requirement | How It Is Satisfied |
| :--- | :--- | :--- |
| **Roll Number** | `MT25022` | Used consistently across all files. |
| **File Naming** | `RollNo_NameOfTheFile.ext` (e.g. `MT25022_Person.h`) | All 20 C++ source/header files use `MT25022_*`. |
| **Excluded Files** | `Readme.md`, `Makefile`, `CMakeLists.txt` | Retain standard names as required by prompt rules. |
| **File Comment Header** | Roll No & Name at the very top of each file | Every header and source file begins with `// Roll No: MT25022`, `// Name: J Chandra Harsha`. |
| **Submission Format** | **ZIP archive only** (`.zip`) | No `.rar` or `.tar.gz`. Google Classroom only accepts `.zip`. |
| **Concurrency Rule** | No locks, semaphores, or condition variables for simulation grid sync | Uses **double buffering** (current & next grids) + `std::thread::join()` and `std::future::wait()`. |

---

## 3. Assignment Requirements & Problem Formulation

### 3.1 The Model
- **Grid:** $N \times N$ discrete grid with 1 individual per cell.
- **States:**
  - `S` (Susceptible): Healthy, vulnerable to infection.
  - `I` (Infected): Contagious, keeps counter $c$ of consecutive days sick.
  - `R` (Recovered): Immune, permanent state.
  - `V` (Vaccinated): Immune, permanent state.
  - `D` (Dead): Permanent state.
- **Neighbourhood:** Moore neighbourhood (8 surrounding adjacent cells, no toroidal wrap-around at boundaries).
- **Update Rules:**
  1. *Infection:* Susceptible cell with $k$ infected neighbours gets infected with probability:
     $$P(k) = 1 - (1 - \beta_{\text{eff}})^k$$
     Newly infected individual starts at $c = 0$.
  2. *Progression:* For infected person, counter increments $c \leftarrow c + 1$. When $c = d$, individual dies with probability $\mu$, otherwise recovers to `R`.
  3. *States `R`, `V`, `D`* never transition.
- **Interventions:**
  - *Masks:* $\beta_{\text{eff}} = \beta \times m$ throughout the entire run ($0 < m \le 1$).
  - *Lockdown:* If active infected fraction $> \theta_{\text{on}}$, scale $\beta_{\text{eff}}$ by $\lambda$ ($0 < \lambda < 1$). When active infected fraction falls below $\theta_{\text{off}}$, lift lockdown.
- **Default Parameters:**
  $N = 1000$, $\beta = 0.08$, $d = 7$, $\mu = 0.01$, $I_0 = 10$, $v = 0$, $m = 1.0$, $\lambda = 0.3$, $\theta_{\text{on}} = 0.02$, $\theta_{\text{off}} = 0.005$, seed $s = 42$, max days $= 365$.

### 3.2 Concurrency & Determinism Constraints
1. **Two Grids (Double Buffering):** All worker threads read only from the `current` grid and write solely into their assigned subgrid in the `next` grid. After all workers finish, pointer/buffer swap occurs. **Zero data races by design**.
2. **Deterministic Randomness:** Standard `rand()` or thread-shared PRNG causes race conditions or non-deterministic ordering. We engineered `MT25022_HashRNG.h`, computing a 64-bit SplitMix hash of `(seed, day, cell_index, event_type)`. This guarantees that **regardless of the number of threads or chunk strategy, the simulation produces 100% identical results.**
3. **Chunking Strategies:**
   - `row_strip`: Horizontal slices of continuous rows.
   - `square_block`: 2D rectangular tiles.
   - `interleaved`: Round-robin interleaved row assignments for optimal load balancing.
4. **Asynchronous Snapshots (Part C1):**
   - Every $K$ days, save population state as a PPM image.
   - Using synchronous blocking I/O freezes the simulation.
   - Implemented an asynchronous snapshot pipeline using `std::future<void>` and an internal bounded queue (max pending snapshots $Q$). The simulation only copies the grid buffer (fast memory copy) and launches the disk I/O in the background via `std::async`.
5. **Parameter Sweep (Part C2):**
   - Parallel exploration over parameter combinations ($\beta$, vaccination rates, random seeds) using `std::future`.
   - Supports timeout limits, error handling, and aggregated CSV reporting.

---

## 4. Predicted Grading Rubric (100 Marks Total)

| Section | Marks | Criteria |
| :--- | :---: | :--- |
| **Part A: Sequential Simulation** | 10 | Accurate disease dynamics, CLI flags, daily CSV generation, checksum calculation. |
| **Part B: Threaded Simulation** | 20 | Thread-per-day, thread pool, 3 spatial decomposition strategies, 100% deterministic equality against sequential run. |
| **Part C: Asynchronous Programming** | 20 | Async PPM snapshot pipeline with bounded pending queue, asynchronous multi-scenario parameter sweep. |
| **Part D: Performance Evaluation** | 20 | Strong scaling, weak scaling, chunk granularity tuning, async I/O overlap analysis. |
| **Part E: Epidemiological Analysis** | 10 | Analysis of infection curves, herd immunity threshold, impact of masks and lockdown interventions. |
| **OOP Architecture & Design** | 10 | Inheritance, polymorphism, templates, RAII, operator overloading, smart pointers, encapsulation. |
| **Code Quality & Robustness** | 5 | Input validation, descriptive custom exceptions, clean naming, header comments. |
| **Documentation & Readme** | 5 | Comprehensive setup instructions, architecture breakdown, concurrency safety proofs. |

---

## 5. Object Oriented Programming (OOP) Mapping Checklist

| OOP Concept | File / Implementation Location | Concrete Usage |
| :--- | :--- | :--- |
| **Inheritance** | [`MT25022_Grid.h`](file:///d:/M%20Tech/oopd/assignment3/MT25022_Grid.h), [`MT25022_Intervention.h`](file:///d:/M%20Tech/oopd/assignment3/MT25022_Intervention.h) | `ChunkStrategy` $\rightarrow$ `RowStripStrategy`, `SquareBlockStrategy`, `InterleavedRowStrategy`.<br>`Intervention` base $\rightarrow$ `MaskIntervention`, `LockdownIntervention`. |
| **Polymorphism** | [`MT25022_Grid.h`](file:///d:/M%20Tech/oopd/assignment3/MT25022_Grid.h), [`MT25022_Intervention.h`](file:///d:/M%20Tech/oopd/assignment3/MT25022_Intervention.h) | Virtual `divide()` for spatial domain decomposition.<br>Virtual `apply()` for daily intervention updates. |
| **Composition** | [`MT25022_Simulation.h`](file:///d:/M%20Tech/oopd/assignment3/MT25022_Simulation.h) | `Simulation` manages `Grid`, `InterventionManager`, `SnapshotWriter`, and `ThreadPool`. |
| **Templates** | [`MT25022_HashRNG.h`](file:///d:/M%20Tech/oopd/assignment3/MT25022_HashRNG.h), [`MT25022_CSVWriter.h`](file:///d:/M%20Tech/oopd/assignment3/MT25022_CSVWriter.h), [`MT25022_ThreadPool.h`](file:///d:/M%20Tech/oopd/assignment3/MT25022_ThreadPool.h) | `HashRNG<T>` for generic hash evaluation.<br>`CSVWriter<MutexType>` for thread-safe output.<br>`ThreadPool::submit<F, Args...>` variadic template. |
| **Exception Handling** | All files | Validates grid bounds, parameter ranges, file paths; throws `std::invalid_argument`, `std::runtime_error`, `std::out_of_range`. |
| **RAII** | [`MT25022_ThreadPool.h`](file:///d:/M%20Tech/oopd/assignment3/MT25022_ThreadPool.h), [`MT25022_CSVWriter.h`](file:///d:/M%20Tech/oopd/assignment3/MT25022_CSVWriter.h) | Thread pool destructor safely completes tasks and joins threads. File streams automatically close on destruction. |
| **Encapsulation** | All classes | All member state is `private` or `protected`; accessed solely through const methods or validated setters. |
| **Operator Overloading** | [`MT25022_Grid.h`](file:///d:/M%20Tech/oopd/assignment3/MT25022_Grid.h), [`MT25022_Person.h`](file:///d:/M%20Tech/oopd/assignment3/MT25022_Person.h) | `Grid::operator()(r, c)` for cell indexing.<br>`Person::operator==` for state comparison.<br>`PopulationCounts::operator+=` for merging thread counts.<br>`operator<<` for formatted logging. |
| **Move Semantics** | [`MT25022_Grid.h`](file:///d:/M%20Tech/oopd/assignment3/MT25022_Grid.h) | `Grid::swap()` uses `std::swap` and move semantics for zero-copy buffer exchange between days. |
| **Smart Pointers** | Simulation & Sweep | `std::unique_ptr<Grid>`, `std::unique_ptr<ChunkStrategy>`, `std::unique_ptr<ThreadPool>`. |
| **Concurrency & Async** | Simulation, ThreadPool, SnapshotWriter | `std::thread`, `std::async`, `std::future`, `std::mutex`, `std::condition_variable` (in pool internal work queue). |

---

## 6. Complete File Directory & Structure

```
d:/M Tech/oopd/assignment3/
│
├── CMakeLists.txt              # Cross-platform CMake build configuration (Windows, Linux, macOS)
├── Makefile                    # Standard GNU Make fallback
├── Readme.md                   # Full user and technical documentation
├── chat.md                     # THIS FILE: Complete session brain & portability log
├── assignment3.txt             # Raw text extraction of assignment specification PDF
├── OOPD____Assignment_3____2026-1.pdf # Original assignment brief
│
├── MT25022_Person.h            # Person class declaration (State enum: S, I, R, V, D; day counter)
├── MT25022_Person.cpp          # Person member implementations and validation
├── MT25022_Grid.h              # Double-buffered N×N Grid, ChunkStrategy inheritance hierarchy
├── MT25022_Grid.cpp            # Grid boundary checks, neighbour infection count, grid swap
├── MT25022_HashRNG.h           # Header-only deterministic SplitMix64 pseudo-random generator
├── MT25022_Intervention.h      # Intervention abstract base, MaskIntervention, LockdownIntervention
├── MT25022_Intervention.cpp    # Threshold checking and transmission rate scaling
├── MT25022_ThreadPool.h        # General-purpose reusable thread pool with work queue
├── MT25022_ThreadPool.cpp      # Worker thread loop, task scheduling, synchronization
├── MT25022_SnapshotWriter.h    # PPM image generation, synchronous & asynchronous pipelines
├── MT25022_SnapshotWriter.cpp  # PPM header/pixel serialization, simulated I/O latency support
├── MT25022_CSVWriter.h         # Header-only templated CSV writer for daily counts & metrics
├── MT25022_Simulation.h        # Central simulation engine coordinating all modules
├── MT25022_Simulation.cpp      # Sequential, thread-per-day, and thread-pool execution loops
├── MT25022_ParameterSweep.h    # Asynchronous multi-configuration sweep runner
├── MT25022_ParameterSweep.cpp  # Concurrent scenario dispatch, future collection, summary CSV
├── MT25022_Main.cpp            # Main CLI binary entry point (pandemic_sim)
├── MT25022_Sweep.cpp           # Parameter sweep CLI binary entry point (sweep)
├── MT25022_Test.cpp            # Correctness verification harness (test_correctness, 54 tests)
│
└── build/                      # Build outputs directory
    ├── pandemic_sim.exe        # Simulation executable
    ├── sweep.exe               # Parameter sweep executable
    └── test_correctness.exe    # Test suite executable
```

---

## 7. How to Build & Run on ANY System

### 7.1 Cross-Platform Build with CMake (Recommended)

#### Option A: Windows (Visual Studio)
```powershell
# In PowerShell:
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release

# Executables will be in:
# build\Release\pandemic_sim.exe
# build\Release\sweep.exe
# build\Release\test_correctness.exe
```

#### Option B: Windows (MinGW-w64 / GCC)
> [!NOTE]
> If your Windows username has spaces (e.g. `J CHANDRA HARSHA`), ensure MinGW is located in a path without spaces (like `C:\mingw64\bin`).
```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

#### Option C: Linux / Ubuntu / Debian / macOS
```bash
# Prerequisites: cmake, g++ (or clang++), make
sudo apt-get update && sudo apt-get install -y cmake build-essential

# Configure and build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Executables will be in build/
./build/pandemic_sim --help
```

### 7.2 Alternative Build: Direct Makefile (Linux / macOS)
```bash
make all        # Builds all 3 binaries
make test       # Runs the automated correctness suite
make clean      # Cleans object files and binaries
```

---

## 8. Command-Line Usage & Quick Reference

### 8.1 Single Run Simulation (`pandemic_sim`)

```bash
# 1. Sequential Mode (Baseline):
./build/pandemic_sim --mode sequential --N 1000 --max-days 365 --seed 42 --csv seq_output.csv

# 2. Thread-Per-Day Mode (4 threads, row strip chunking):
./build/pandemic_sim --mode thread_per_day --threads 4 --chunks 4 --chunk-strategy row_strip --csv tpd_output.csv

# 3. Thread Pool Mode (4 threads, 16 square block chunks):
./build/pandemic_sim --mode thread_pool --threads 4 --chunks 16 --chunk-strategy square_block --csv pool_output.csv

# 4. Interleaved Rows Strategy (8 threads):
./build/pandemic_sim --mode thread_pool --threads 8 --chunks 8 --chunk-strategy interleaved

# 5. Asynchronous Snapshot Writing (Every 10 days, max 4 pending):
./build/pandemic_sim --mode thread_pool --snapshot-interval 10 --async-snapshots --max-pending 4 --snapshot-dir snapshots

# 6. With Simulated Disk I/O Latency (50ms):
./build/pandemic_sim --snapshot-interval 5 --async-snapshots --io-latency 50
```

### 8.2 Parameter Sweep Tool (`sweep`)

```bash
./build/sweep --N 500 --max-days 200 \
  --sweep-betas 0.04,0.06,0.08,0.10,0.12 \
  --sweep-vaccines 0.0,0.1,0.2,0.3,0.5 \
  --sweep-seeds 42,43,44,45,46 \
  --sweep-output sweep_results.csv \
  --sweep-time-limit 300
```

### 8.3 Automated Verification Suite (`test_correctness`)

```bash
./build/test_correctness --N 100 --max-days 50
```

---

## 9. Verification & Deterministic Equality Achieved

During today's test session:
1. **Identical Results Confirmed:**
   - Sequential run vs Thread Pool (4 threads, square block chunking) produced:
     - Peak infection: **2.09%** on Day **34**
     - Total infected individuals: **826**
     - Total deaths: **8**
     - 64-bit Checksum: `11440066197921461018`
     - CSV line-by-line diff: **IDENTICAL (0 differences)**
2. **Correctness Suite Results:**
   - **54 out of 54 tests PASSED**.
   - Tested thread counts: $T \in \{1, 2, 3, 4, 7, 8, 16\}$.
   - Tested chunk strategies: `row_strip`, `square_block`, `interleaved`.
   - Tested execution modes: `thread_per_day`, `thread_pool`.
   - Result: Every single parallel run matched the sequential run byte-for-byte.

---

## 10. Next Steps / Continuation Roadmap for Your Other System

When you open this repository on your other computer:

### Step 1: Clone / Copy & Verify Compilation
1. Copy the `assignment3` folder over.
2. Open terminal/PowerShell in the folder and execute:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ./build/test_correctness
   ```
3. Ensure all 54 tests pass.

### Step 2: Part D – Performance Experiments (To Complete Report)
To write the final report, execute these benchmarks:
1. **Strong Scaling:**
   - Fix $N = 1000$, days $= 100$.
   - Measure execution time for threads $T \in \{1, 2, 4, 8, 16\}$.
   - Calculate speedup $S(T) = T_1 / T_T$ and parallel efficiency $E(T) = S(T) / T$.
2. **Weak Scaling:**
   - Scale work proportionally to threads (e.g. $N = 500 \times \sqrt{T}$).
   - Measure whether execution time remains constant.
3. **Chunk Granularity (Chunk Size Optimization):**
   - For a fixed 8-thread run, vary chunk count $C \in \{8, 16, 32, 64, 128\}$.
   - Compare `row_strip` vs `square_block` vs `interleaved`.
4. **Asynchronous I/O Overlap:**
   - Compare total runtime with `--snapshot-interval 5` under:
     - Synchronous mode (no `--async-snapshots`)
     - Asynchronous mode (`--async-snapshots --max-pending 4`)
     - Vary `--io-latency` (0ms, 20ms, 50ms, 100ms) to demonstrate that the asynchronous pipeline completely hides slow I/O beneath grid computation.

### Step 3: Part E – Epidemiological Analysis
Run simulations with varying:
- $\beta \in [0.02, 0.16]$: Find the critical epidemic threshold $R_0 = 1$.
- Vaccination fraction $v \in [0.0, 0.8]$: Show herd immunity (where epidemic fails to take off).
- Masks ($m = 0.5$) vs Lockdown ($\lambda = 0.3, \theta_{\text{on}} = 0.02$): Compare how each flattens the curve.

### Step 4: Python Script for Report Graphs (Optional Helper)
You can run this quick Python script to generate plots from your output CSVs:
```python
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("output.csv")
plt.figure(figsize=(10, 6))
plt.plot(df['day'], df['I_count'], label='Infected (I)', color='red')
plt.plot(df['day'], df['S_count'], label='Susceptible (S)', color='blue')
plt.plot(df['day'], df['R_count'], label='Recovered (R)', color='green')
plt.plot(df['day'], df['D_count'], label='Dead (D)', color='black')
plt.xlabel("Day")
plt.ylabel("Population Count")
plt.title("Pandemic Spread Curves (N=1000)")
plt.legend()
plt.grid(True)
plt.savefig("pandemic_curve.png")
```

### Step 5: Final Submission Packaging
Create the final submission archive:
- Name: `MT25022_Assignment3.zip`
- Ensure it contains all `MT25022_*` files, `Readme.md`, `Makefile`, `CMakeLists.txt`, and your final PDF report.
- Submit only via Google Classroom as specified.

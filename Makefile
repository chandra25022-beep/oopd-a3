# MT25022 - J Chandra Harsha
# Makefile for Pandemic Spread Simulation
# OOPD Assignment 3

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -pthread

# Source files (excluding main files)
COMMON_SRC = MT25022_Person.cpp MT25022_Grid.cpp MT25022_Intervention.cpp \
             MT25022_ThreadPool.cpp MT25022_SnapshotWriter.cpp MT25022_Simulation.cpp

COMMON_OBJ = $(COMMON_SRC:.cpp=.o)

# Targets
MAIN_TARGET = pandemic_sim
SWEEP_TARGET = sweep
TEST_TARGET = test_correctness

.PHONY: all clean test sweep_run help

all: $(MAIN_TARGET) $(SWEEP_TARGET) $(TEST_TARGET)

# Main simulation executable
$(MAIN_TARGET): MT25022_Main.o $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Parameter sweep executable
$(SWEEP_TARGET): MT25022_Sweep.o $(COMMON_OBJ) MT25022_ParameterSweep.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Test executable
$(TEST_TARGET): MT25022_Test.o $(COMMON_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Generic rule for object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run tests (Part B verification)
test: $(TEST_TARGET)
	./$(TEST_TARGET) --N 100 --max-days 50 --seed 42

# Run a default sequential simulation
run_seq: $(MAIN_TARGET)
	./$(MAIN_TARGET) --mode sequential --N 200 --max-days 100 --csv seq_output.csv

# Run threaded simulation
run_threaded: $(MAIN_TARGET)
	./$(MAIN_TARGET) --mode thread_pool --threads 4 --chunks 8 --N 200 --max-days 100 --csv threaded_output.csv

# Run with snapshots (blocking)
run_snap_block: $(MAIN_TARGET)
	./$(MAIN_TARGET) --mode thread_pool --threads 4 --N 200 --max-days 100 \
		--snapshot-interval 10 --csv snap_block_output.csv

# Run with snapshots (async)
run_snap_async: $(MAIN_TARGET)
	./$(MAIN_TARGET) --mode thread_pool --threads 4 --N 200 --max-days 100 \
		--snapshot-interval 10 --async-snapshots --max-pending 4 --csv snap_async_output.csv

# Run parameter sweep
sweep_run: $(SWEEP_TARGET)
	./$(SWEEP_TARGET) --N 200 --max-days 100 \
		--sweep-betas 0.04,0.06,0.08,0.10,0.12 \
		--sweep-vaccines 0.0,0.1,0.2,0.3,0.5 \
		--sweep-seeds 42,43,44 \
		--sweep-output sweep_results.csv

# Performance study (Part D)
run_perf_strong: $(MAIN_TARGET)
	@echo "=== Strong Scaling (N=4000, 200 days) ==="
	@for T in 1 2 4 8 16; do \
		echo "--- T=$$T ---"; \
		./$(MAIN_TARGET) --mode thread_pool --threads $$T --N 4000 --max-days 200 \
			--csv perf_strong_T$$T.csv 2>&1 | grep "Elapsed"; \
	done

run_perf_weak: $(MAIN_TARGET)
	@echo "=== Weak Scaling (1000x1000 per thread) ==="
	@for T in 1 2 4 8; do \
		N=$$((1000 * T)); \
		N=$$(echo "sqrt($$N * 1000)" | bc); \
		echo "--- T=$$T, N=$$N ---"; \
		./$(MAIN_TARGET) --mode thread_pool --threads $$T --N $$N --max-days 100 \
			--csv perf_weak_T$$T.csv 2>&1 | grep "Elapsed"; \
	done

# Clean build artifacts
clean:
	rm -f *.o $(MAIN_TARGET) $(SWEEP_TARGET) $(TEST_TARGET)
	rm -f *.exe
	rm -f test_*.csv sweep_run_*.csv
	rm -rf snapshots/

# Help
help:
	@echo "Available targets:"
	@echo "  all            - Build all executables"
	@echo "  pandemic_sim   - Build main simulation"
	@echo "  sweep          - Build parameter sweep"
	@echo "  test_correctness - Build correctness test"
	@echo "  test           - Run correctness tests"
	@echo "  run_seq        - Run sequential simulation"
	@echo "  run_threaded   - Run threaded simulation"
	@echo "  run_snap_block - Run with blocking snapshots"
	@echo "  run_snap_async - Run with async snapshots"
	@echo "  sweep_run      - Run parameter sweep"
	@echo "  clean          - Clean build artifacts"

# Dependencies
MT25022_Main.o: MT25022_Main.cpp MT25022_Simulation.h
MT25022_Sweep.o: MT25022_Sweep.cpp MT25022_Simulation.h MT25022_ParameterSweep.h
MT25022_Test.o: MT25022_Test.cpp MT25022_Simulation.h
MT25022_Person.o: MT25022_Person.cpp MT25022_Person.h
MT25022_Grid.o: MT25022_Grid.cpp MT25022_Grid.h MT25022_Person.h MT25022_HashRNG.h
MT25022_Intervention.o: MT25022_Intervention.cpp MT25022_Intervention.h
MT25022_ThreadPool.o: MT25022_ThreadPool.cpp MT25022_ThreadPool.h
MT25022_SnapshotWriter.o: MT25022_SnapshotWriter.cpp MT25022_SnapshotWriter.h MT25022_Person.h
MT25022_Simulation.o: MT25022_Simulation.cpp MT25022_Simulation.h MT25022_Grid.h MT25022_Intervention.h MT25022_SnapshotWriter.h MT25022_CSVWriter.h MT25022_ThreadPool.h
MT25022_ParameterSweep.o: MT25022_ParameterSweep.cpp MT25022_ParameterSweep.h MT25022_Simulation.h

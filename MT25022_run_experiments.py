#!/usr/bin/env python3
# MT25022 - J Chandra Harsha
# Automated Experiment Runner & Analyzer for Parts D & E
# Generates benchmark CSVs, Markdown tables, and standalone SVG charts without external dependencies.

import os
import sys
import time
import subprocess
import argparse
import math
import csv

SIM_BIN = "./pandemic_sim"
SWEEP_BIN = "./sweep"

def run_cmd(cmd):
    p = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if p.returncode != 0:
        print(f"Command failed: {cmd}\nStderr: {p.stderr}")
    return p.stdout

def parse_summary(stdout):
    results = {}
    for line in stdout.splitlines():
        line = line.strip()
        if "Days simulated:" in line:
            results["days"] = int(line.split(":")[1].strip())
        elif "Peak infected:" in line:
            val = line.split(":")[1].strip().split("%")[0].strip()
            results["peak_pct"] = float(val)
        elif "Total infected:" in line:
            results["total_infected"] = int(line.split(":")[1].strip())
        elif "Total deaths:" in line:
            results["deaths"] = int(line.split(":")[1].strip())
        elif "Elapsed time:" in line:
            val = line.split(":")[1].strip().split()[0]
            results["elapsed_ms"] = float(val)
        elif "Final checksum:" in line:
            results["checksum"] = line.split(":")[1].strip()
    return results

def karp_flatt(speedup, threads):
    if threads <= 1 or speedup <= 0:
        return 0.0
    return ((1.0 / speedup) - (1.0 / threads)) / (1.0 - (1.0 / threads))

def generate_svg_line_chart(title, x_label, y_label, series_data, filename, width=700, height=400):
    """
    series_data: dict of {series_name: [(x, y), ...]}
    Generates a clean standalone SVG chart.
    """
    margin = {"top": 50, "right": 140, "bottom": 50, "left": 70}
    plot_w = width - margin["left"] - margin["right"]
    plot_h = height - margin["top"] - margin["bottom"]

    all_x = [pt[0] for pts in series_data.values() for pt in pts]
    all_y = [pt[1] for pts in series_data.values() for pt in pts]

    if not all_x or not all_y:
        return

    min_x, max_x = min(all_x), max(all_x)
    min_y = min(0.0, min(all_y))
    max_y = max(all_y) * 1.15 if max(all_y) > 0 else 1.0

    def sx(val):
        if max_x == min_x: return margin["left"] + plot_w / 2
        return margin["left"] + ((val - min_x) / (max_x - min_x)) * plot_w

    def sy(val):
        if max_y == min_y: return margin["top"] + plot_h / 2
        return margin["top"] + (1.0 - (val - min_y) / (max_y - min_y)) * plot_h

    colors = ["#2563eb", "#dc2626", "#16a34a", "#9333ea", "#ea580c"]
    
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" style="background:#ffffff; font-family:sans-serif;">']
    svg.append(f'<text x="{width/2}" y="30" text-anchor="middle" font-size="16" font-weight="bold" fill="#1e293b">{title}</text>')
    
    # Axes
    svg.append(f'<line x1="{margin["left"]}" y1="{margin["top"]+plot_h}" x2="{margin["left"]+plot_w}" y2="{margin["top"]+plot_h}" stroke="#94a3b8" stroke-width="2"/>')
    svg.append(f'<line x1="{margin["left"]}" y1="{margin["top"]}" x2="{margin["left"]}" y2="{margin["top"]+plot_h}" stroke="#94a3b8" stroke-width="2"/>')

    # Grid lines and labels
    for i in range(5):
        y_val = min_y + (max_y - min_y) * (i / 4.0)
        y_pos = sy(y_val)
        svg.append(f'<line x1="{margin["left"]}" y1="{y_pos}" x2="{margin["left"]+plot_w}" y2="{y_pos}" stroke="#e2e8f0" stroke-width="1" stroke-dasharray="4"/>')
        svg.append(f'<text x="{margin["left"]-8}" y="{y_pos+4}" text-anchor="end" font-size="11" fill="#64748b">{y_val:.2f}</text>')

    svg.append(f'<text x="{margin["left"] + plot_w/2}" y="{height-12}" text-anchor="middle" font-size="12" font-weight="bold" fill="#475569">{x_label}</text>')
    svg.append(f'<text transform="rotate(-90)" x="-{margin["top"] + plot_h/2}" y="20" text-anchor="middle" font-size="12" font-weight="bold" fill="#475569">{y_label}</text>')

    # Plot series
    c_idx = 0
    leg_y = margin["top"] + 10
    for name, pts in series_data.items():
        color = colors[c_idx % len(colors)]
        c_idx += 1
        
        # Polyline
        coords = [f"{sx(pt[0]):.1f},{sy(pt[1]):.1f}" for pt in pts]
        svg.append(f'<polyline fill="none" stroke="{color}" stroke-width="2.5" points="{" ".join(coords)}"/>')
        
        # Points
        for pt in pts:
            px, py = sx(pt[0]), sy(pt[1])
            svg.append(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="4" fill="{color}"/>')

        # Legend
        svg.append(f'<rect x="{margin["left"]+plot_w+15}" y="{leg_y-8}" width="12" height="12" fill="{color}"/>')
        svg.append(f'<text x="{margin["left"]+plot_w+32}" y="{leg_y+2}" font-size="11" fill="#334155">{name}</text>')
        leg_y += 20

    svg.append('</svg>')
    
    with open(filename, "w") as f:
        f.write("\n".join(svg))
    print(f"Chart saved: {filename}")


# ================= PART D1: Strong Scaling =================
def run_part_d1(quick=False):
    print("\n" + "="*60)
    print("PART D1: Strong Scaling (Fixed Problem Size N)")
    print("="*60)
    N = 500 if quick else 1000
    days = 30 if quick else 100
    threads_list = [1, 2, 4, 8] if quick else [1, 2, 4, 8, 16]
    
    modes = ["thread_pool", "thread_per_day"]
    results = {m: [] for m in modes}

    print(f"Running Strong Scaling with N={N}, days={days}")
    for mode in modes:
        print(f"\n--- Mode: {mode} ---")
        t1_time = None
        for T in threads_list:
            cmd = f"{SIM_BIN} --mode {mode} --threads {T} --chunks {T} --N {N} --max-days {days} --seed 42 --csv tmp.csv"
            out = run_cmd(cmd)
            res = parse_summary(out)
            ms = res.get("elapsed_ms", 0.0)
            if T == 1 or t1_time is None:
                t1_time = ms
            speedup = (t1_time / ms) if ms > 0 else 0.0
            efficiency = (speedup / T) if T > 0 else 0.0
            kf = karp_flatt(speedup, T)
            results[mode].append((T, ms, speedup, efficiency, kf))
            print(f"T={T:2d} | Time: {ms:8.2f} ms | Speedup: {speedup:5.2f}x | Efficiency: {efficiency:5.2f} | Karp-Flatt e: {kf:.4f}")

    # Output markdown table
    print("\n### Part D1 Results Table")
    print("| Threads | Mode | Time (ms) | Speedup | Efficiency | Karp-Flatt (e) |")
    print("|:-------:|:----:|:---------:|:-------:|:----------:|:--------------:|")
    for mode in modes:
        for r in results[mode]:
            print(f"| {r[0]} | {mode} | {r[1]:.2f} | {r[2]:.2f}x | {r[3]:.2f} | {r[4]:.4f} |")

    # Generate charts
    speedup_data = {m: [(r[0], r[2]) for r in results[m]] for m in modes}
    speedup_data["Linear Ideal"] = [(t, float(t)) for t in threads_list]
    generate_svg_line_chart(f"Strong Scaling Speedup (N={N})", "Threads (T)", "Speedup S(T)", speedup_data, "chart_d1_speedup.svg")

    eff_data = {m: [(r[0], r[3]) for r in results[m]] for m in modes}
    eff_data["Ideal (1.0)"] = [(t, 1.0) for t in threads_list]
    generate_svg_line_chart(f"Strong Scaling Efficiency (N={N})", "Threads (T)", "Efficiency E(T)", eff_data, "chart_d1_efficiency.svg")


# ================= PART D3: Chunk Size & Strategies =================
def run_part_d3(quick=False):
    print("\n" + "="*60)
    print("PART D3: Chunk Strategies & Granularity Study")
    print("="*60)
    N = 500 if quick else 1000
    days = 30 if quick else 60
    T = 4
    chunk_multipliers = [1, 2, 4, 8] if quick else [1, 2, 4, 8, 16]
    strategies = ["row_strip", "square_block", "interleaved"]
    
    results = {s: [] for s in strategies}

    for strat in strategies:
        print(f"\n--- Strategy: {strat} (T={T}) ---")
        for m in chunk_multipliers:
            C = T * m
            cmd = f"{SIM_BIN} --mode thread_pool --threads {T} --chunks {C} --chunk-strategy {strat} --N {N} --max-days {days} --seed 42 --csv tmp.csv"
            out = run_cmd(cmd)
            res = parse_summary(out)
            ms = res.get("elapsed_ms", 0.0)
            results[strat].append((C, ms))
            print(f"Strategy: {strat:12s} | Chunks C={C:2d} ({m}T) | Time: {ms:8.2f} ms")

    print("\n### Part D3 Results Table")
    print("| Chunks (C) | row_strip (ms) | square_block (ms) | interleaved (ms) |")
    print("|:----------:|:--------------:|:-----------------:|:----------------:|")
    for i in range(len(chunk_multipliers)):
        c_val = results["row_strip"][i][0]
        t_row = results["row_strip"][i][1]
        t_sq = results["square_block"][i][1]
        t_intl = results["interleaved"][i][1]
        print(f"| {c_val} | {t_row:.2f} | {t_sq:.2f} | {t_intl:.2f} |")

    chart_data = {s: [(r[0], r[1]) for r in results[s]] for s in strategies}
    generate_svg_line_chart(f"Execution Time vs Chunk Count (T={T}, N={N})", "Chunks (C)", "Time (ms)", chart_data, "chart_d3_chunks.svg")


# ================= PART D5: I/O Hiding =================
def run_part_d5(quick=False):
    print("\n" + "="*60)
    print("PART D5: I/O Latency Hiding (Blocking vs Async Snapshots)")
    print("="*60)
    N = 200
    days = 30
    K = 5
    latencies = [0, 10, 20, 50] if quick else [0, 10, 20, 50, 100]

    sync_times = []
    async_times = []

    for lat in latencies:
        # Blocking
        cmd_sync = f"{SIM_BIN} --mode sequential --N {N} --max-days {days} --snapshot-interval {K} --io-latency {lat} --snapshot-dir snap_s --csv tmp.csv"
        out_s = run_cmd(cmd_sync)
        t_sync = parse_summary(out_s).get("elapsed_ms", 0.0)
        sync_times.append((lat, t_sync))

        # Async
        cmd_async = f"{SIM_BIN} --mode sequential --N {N} --max-days {days} --snapshot-interval {K} --io-latency {lat} --async-snapshots --snapshot-dir snap_a --csv tmp.csv"
        out_a = run_cmd(cmd_async)
        t_async = parse_summary(out_a).get("elapsed_ms", 0.0)
        async_times.append((lat, t_async))

        speedup = (t_sync / t_async) if t_async > 0 else 1.0
        saved_ms = t_sync - t_async
        print(f"Latency: {lat:3d} ms | Sync: {t_sync:7.2f} ms | Async: {t_async:7.2f} ms | Speedup: {speedup:4.2f}x | Saved: {saved_ms:7.2f} ms")

    print("\n### Part D5 Results Table")
    print("| Latency (ms) | Sync Time (ms) | Async Time (ms) | Async Speedup | Time Saved (ms) |")
    print("|:------------:|:--------------:|:---------------:|:-------------:|:---------------:|")
    for i in range(len(latencies)):
        lat = latencies[i]
        ts = sync_times[i][1]
        ta = async_times[i][1]
        sp = (ts / ta) if ta > 0 else 1.0
        print(f"| {lat} | {ts:.2f} | {ta:.2f} | {sp:.2f}x | {ts - ta:.2f} |")

    chart_data = {
        "Blocking (Sync)": sync_times,
        "Asynchronous": async_times
    }
    generate_svg_line_chart(f"I/O Latency Hiding (Snapshot K={K}, N={N})", "Simulated I/O Latency (ms)", "Total Execution Time (ms)", chart_data, "chart_d5_io_hiding.svg")


# ================= PART E: Epidemiology Analysis =================
def run_part_e(quick=False):
    print("\n" + "="*60)
    print("PART E: Epidemiological Simulation & Interventions")
    print("="*60)
    N = 300 if quick else 500
    days = 60 if quick else 100

    scenarios = [
        ("No Intervention", "--mask-factor 1.0 --lockdown-theta-on 0.99 --lockdown-theta-off 0.01"),
        ("Masks (m=0.5)", "--mask-factor 0.5 --lockdown-theta-on 0.99 --lockdown-theta-off 0.01"),
        ("Lockdown (theta=0.02)", "--mask-factor 1.0 --lockdown-theta-on 0.02 --lockdown-theta-off 0.005"),
        ("Combined (Masks+Lockdown)", "--mask-factor 0.5 --lockdown-theta-on 0.02 --lockdown-theta-off 0.005")
    ]

    print(f"Running 4 Intervention Scenarios (N={N}, days={days})")
    curve_data = {}

    for name, flags in scenarios:
        csv_file = f"epi_{name.replace(' ', '_').replace('(', '').replace(')', '').replace('=', '').lower()}.csv"
        cmd = f"{SIM_BIN} --mode thread_pool --threads 4 --N {N} --max-days {days} {flags} --seed 42 --csv {csv_file}"
        out = run_cmd(cmd)
        summary = parse_summary(out)
        print(f"Scenario: {name:25s} | Peak Infected: {summary.get('peak_pct', 0.0):.2f}% | Total Infected: {summary.get('total_infected', 0):6d} | Deaths: {summary.get('deaths', 0):4d}")

        # Read CSV to get I(t) curve
        pts = []
        if os.path.exists(csv_file):
            with open(csv_file, "r") as f:
                reader = csv.DictReader(f)
                for row in reader:
                    d = int(row["day"])
                    inf = int(row["I"])
                    pts.append((d, (inf / (N * N)) * 100.0))
        curve_data[name] = pts

    generate_svg_line_chart("Epidemic Curve I(t) Under Different Interventions", "Day", "Infected Population (%)", curve_data, "chart_e_interventions.svg")


def cleanup():
    for f in ["tmp.csv", "out_sync.csv", "out_async.csv"]:
        if os.path.exists(f): os.remove(f)
    for d in ["snap_s", "snap_a", "snapshots"]:
        if os.path.exists(d):
            import shutil
            shutil.rmtree(d, ignore_errors=True)

def main():
    parser = argparse.ArgumentParser(description="OOPD A3 Experiment Runner")
    parser.add_argument("--quick", action="store_true", help="Run quick benchmarks for live demo")
    parser.add_argument("--all", action="store_true", help="Run all parts D1, D3, D5, and E")
    parser.add_argument("--part", choices=["D1", "D3", "D5", "E"], help="Run specific experiment part")
    args = parser.parse_args()

    if not os.path.exists(SIM_BIN):
        print(f"Binary '{SIM_BIN}' not found. Please run 'make all' first.")
        sys.exit(1)

    try:
        if args.part == "D1" or args.all:
            run_part_d1(args.quick)
        if args.part == "D3" or args.all:
            run_part_d3(args.quick)
        if args.part == "D5" or args.all:
            run_part_d5(args.quick)
        if args.part == "E" or args.all:
            run_part_e(args.quick)
        if not args.all and not args.part:
            print("Please specify --all or --part [D1|D3|D5|E]. Add --quick for rapid execution.")
    finally:
        cleanup()

if __name__ == "__main__":
    main()

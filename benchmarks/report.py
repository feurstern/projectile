#!/usr/bin/env python3
"""Summarizes projectile_benchmarks results as Markdown tables.

    report.py RUN.json              the environment, then each benchmark's median time,
                                    throughput, time per item and coefficient of variation
    report.py RUN.json OTHER.json   also compares the two runs, benchmark by benchmark

RUN.json comes from

    projectile_benchmarks --benchmark_repetitions=10 --benchmark_report_aggregates_only=true \\
        --benchmark_out=RUN.json --benchmark_out_format=json

so that it holds the median and coefficient-of-variation aggregates of each benchmark. See
docs/benchmarks.md for the full procedure. Needs only the Python standard library.
"""

import argparse
import json
import statistics
import sys

TIME_UNITS = {"ns": 1e-9, "us": 1e-6, "ms": 1e-3, "s": 1.0}


def load(path):
    """Returns (context, {benchmark name: {aggregate name: entry}}) from a JSON results file."""
    with open(path, encoding="utf-8") as file:
        data = json.load(file)
    results = {}
    for entry in data.get("benchmarks", []):
        if entry.get("run_type") != "aggregate":
            continue
        if entry.get("error_occurred"):
            sys.exit(f"{path}: {entry['run_name']} failed: {entry.get('error_message', '')}")
        results.setdefault(entry["run_name"], {})[entry["aggregate_name"]] = entry
    if not results:
        sys.exit(
            f"{path}: no aggregates found; run the benchmarks with --benchmark_repetitions"
        )

    # With random interleaving, the file lists benchmarks in the order their last repetition ran.
    # Report them in the order they were registered instead.
    def registration_order(item):
        entry = next(iter(item[1].values()))
        return entry.get("family_index", 0), entry.get("per_family_instance_index", 0)

    return data.get("context", {}), dict(sorted(results.items(), key=registration_order))


def seconds(entry):
    """An entry's real time, in seconds."""
    return entry["real_time"] * TIME_UNITS[entry["time_unit"]]


def format_time(value):
    for unit, scale in (("s", 1.0), ("ms", 1e-3), ("µs", 1e-6)):
        if value >= scale:
            return f"{value / scale:.3g} {unit}"
    return f"{value / 1e-9:.3g} ns"


def format_rate(value):
    for prefix, scale in (("G", 1e9), ("M", 1e6), ("k", 1e3)):
        if value >= scale:
            return f"{value / scale:.3g} {prefix}/s"
    return f"{value:.3g} /s"


def cache_text(cache, num_cpus):
    """A cache as Google Benchmark's console output shows it: size and number of instances."""
    sharing = cache.get("num_sharing", 0)
    instances = num_cpus // sharing if sharing and num_cpus else 0
    count = f" (×{instances})" if instances > 1 else ""
    return f"L{cache['level']} {cache['type'].lower()} {cache['size'] // 1024} KiB{count}"


def environment(context, runs):
    num_cpus = context.get("num_cpus", 0)
    caches = ", ".join(cache_text(cache, num_cpus) for cache in context.get("caches", []))
    scaling = "enabled" if context.get("cpu_scaling_enabled") else "disabled"
    lines = [
        f"- **CPU:** {context.get('cpu_model', 'unknown')}, {num_cpus or '?'} logical CPUs, "
        f"reported clock {context.get('mhz_per_cpu', '?')} MHz, frequency scaling {scaling}",
        f"- **Caches:** {caches or 'unknown'}",
        f"- **Compiler:** {context.get('compiler', 'unknown')}; flags `"
        f"{context.get('cxx_flags', '') or '(none)'}`; build type "
        f"{context.get('build_type', 'unknown')}; Projectile as a "
        f"{context.get('projectile_library', 'unknown').replace('_', ' ').lower()}",
        f"- **Projectile:** {context.get('projectile_version', 'unknown')}; Google Benchmark "
        f"library built as {context.get('library_build_type', 'unknown')}",
        f"- **Runs:** {runs}",
    ]
    return "\n".join(lines)


def summary_table(results):
    rows = [
        "| Benchmark | Median time | Throughput | Per item | CV |",
        "|---|--:|--:|--:|--:|",
    ]
    for name, aggregates in results.items():
        median = aggregates.get("median")
        if median is None:
            continue
        rate = median.get("items_per_second")
        cv = aggregates.get("cv")
        # The coefficient of variation is a fraction: the standard deviation over the mean.
        cv_text = f"{cv['real_time'] * 100:.1f}%" if cv else "–"
        rate_text = format_rate(rate) if rate else "–"
        per_item = format_time(1.0 / rate) if rate else "–"
        rows.append(
            f"| `{name}` | {format_time(seconds(median))} | {rate_text} | {per_item} | {cv_text} |"
        )
    return "\n".join(rows)


def comparison_table(first, second):
    rows = [
        "| Benchmark | First run | Second run | Difference |",
        "|---|--:|--:|--:|",
    ]
    differences = []
    for name, aggregates in first.items():
        if name not in second or "median" not in aggregates or "median" not in second[name]:
            continue
        a = seconds(aggregates["median"])
        b = seconds(second[name]["median"])
        difference = (b - a) / a * 100.0
        differences.append((abs(difference), name))
        rows.append(
            f"| `{name}` | {format_time(a)} | {format_time(b)} | {difference:+.1f}% |"
        )
    if differences:
        largest, largest_name = max(differences)
        typical = statistics.median(value for value, _ in differences)
        rows.append("")
        rows.append(
            f"Median difference {typical:.1f}%; largest {largest:.1f}% (`{largest_name}`)."
        )
    return "\n".join(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("run", help="JSON results of a run")
    parser.add_argument("other", nargs="?", help="JSON results of a second run, to compare")
    arguments = parser.parse_args()

    context, results = load(arguments.run)
    repetitions = max(
        (entry.get("repetitions", 0) for aggregates in results.values()
         for entry in aggregates.values()),
        default=0,
    )
    runs = f"{context.get('date', 'unknown date')}, {repetitions} repetitions per benchmark"
    print("### Environment\n")
    print(environment(context, runs))
    print("\n### Results\n")
    print(summary_table(results))
    if arguments.other:
        _, other = load(arguments.other)
        print("\n### Run-to-run comparison\n")
        print(comparison_table(results, other))


if __name__ == "__main__":
    main()

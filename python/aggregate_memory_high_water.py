"""Aggregate per-rank memory high-water JSON files into a versioned summary."""

import argparse
import json
import re
from pathlib import Path


SCHEMA_PER_RANK = "lagrangian-amr.memory-high-water.per-rank.v1"
SCHEMA_AGGREGATE = "lagrangian-amr.memory-high-water.aggregate.v1"

MAX_FIELDS = (
    "max_generation", "max_step", "max_local_leaves", "max_ghost_leaves",
    "max_mirror_leaves", "max_logical_send_entries", "max_local_payload_bytes",
    "max_ghost_payload_buffer_bytes", "max_estimated_local_payload_bytes",
    "max_estimated_ghost_payload_bytes", "max_logical_send_payload_bytes",
    "max_logical_receive_payload_bytes", "max_estimated_send_payload_bytes",
    "max_estimated_receive_payload_bytes", "max_p4est_reported_ghost_bytes",
)
SUM_FIELDS = (
    "completed_exchange_count", "cumulative_logical_send_payload_bytes",
    "cumulative_logical_receive_payload_bytes",
    "cumulative_estimated_send_payload_bytes",
    "cumulative_estimated_receive_payload_bytes",
)
REQUIRED_VALUES = tuple(set(MAX_FIELDS) | set(SUM_FIELDS))


def discover_rank_files(directory: Path):
    pattern = re.compile(r"memory_high_water_rank_(\d+)\.json$")
    found = {}
    for path in sorted(directory.glob("memory_high_water_rank_*.json")):
        match = pattern.match(path.name)
        if not match:
            continue
        found[int(match.group(1))] = path
    return found


def validate_rank(rank, payload, expected_ranks, expected_size):
    if payload.get("schema") != SCHEMA_PER_RANK:
        raise ValueError(f"rank {rank}: bad schema")
    if payload.get("rank") != rank:
        raise ValueError(f"rank {rank}: mismatched rank field")
    size = payload.get("size")
    if not isinstance(size, int) or size < 1:
        raise ValueError(f"rank {rank}: bad size")
    if expected_size is not None and size != expected_size:
        raise ValueError(f"rank {rank}: expected size {expected_size}, got {size}")
    coverage = payload.get("coverage_bits")
    if not isinstance(coverage, int) or coverage < 1 or coverage > 7:
        raise ValueError(f"rank {rank}: bad coverage_bits {coverage!r}")
    values = payload.get("values")
    if not isinstance(values, dict):
        raise ValueError(f"rank {rank}: missing values")
    missing = [name for name in REQUIRED_VALUES if name not in values]
    if missing:
        raise ValueError(f"rank {rank}: missing fields {missing}")
    for name in REQUIRED_VALUES:
        value = values[name]
        if not isinstance(value, int) or value < 0:
            raise ValueError(f"rank {rank}: bad {name}={value!r}")
    return size


def aggregate(directory: Path, expected_ranks=None, expected_size=None,
              output: Path = None):
    files = discover_rank_files(directory)
    if expected_ranks is not None:
        if set(files) != set(range(expected_ranks)):
            raise ValueError(
                f"expected ranks 0..{expected_ranks - 1}, got {sorted(files)}")
    if not files:
        raise ValueError("no per-rank memory high-water files found")

    ranks = []
    global_max = {name: 0 for name in MAX_FIELDS}
    global_sum = {name: 0 for name in SUM_FIELDS}
    size_set = set()
    total_exchanges = 0
    coverage_union = 0

    for rank, path in sorted(files.items()):
        with path.open("r", encoding="utf-8") as handle:
            payload = json.load(handle)
        size = validate_rank(rank, payload, expected_ranks, expected_size)
        size_set.add(size)
        coverage_union |= int(payload["coverage_bits"])
        values = payload["values"]
        for name in MAX_FIELDS:
            global_max[name] = max(global_max[name], values[name])
        for name in SUM_FIELDS:
            global_sum[name] += values[name]
        total_exchanges += values["completed_exchange_count"]
        ranks.append({
            "rank": rank,
            "size": size,
            "coverage_bits": int(payload["coverage_bits"]),
            "values": values,
        })

    if expected_size is not None and size_set != {expected_size}:
        raise ValueError(f"expected uniform size {expected_size}, got {sorted(size_set)}")

    aggregate_payload = {
        "schema": SCHEMA_AGGREGATE,
        "rank_count": len(ranks),
        "size_set": sorted(size_set),
        "coverage_union_bits": coverage_union,
        "total_completed_exchange_count": total_exchanges,
        "global_max": global_max,
        "global_sum": global_sum,
        "ranks": ranks,
    }
    if output is None:
        output = directory / "memory_high_water_aggregate.json"
    output.write_text(json.dumps(aggregate_payload, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    return aggregate_payload


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dir", type=Path, default=Path("output"))
    parser.add_argument("--expected-ranks", type=int)
    parser.add_argument("--expected-size", type=int)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        result = aggregate(args.dir, args.expected_ranks, args.expected_size,
                           args.output)
    except Exception as error:
        print(f"AGGREGATE FAIL: {error}", flush=True)
        return 1
    print(f"AGGREGATE PASS: ranks={result['rank_count']} "
          f"coverage={result['coverage_union_bits']}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
#!/usr/bin/env python3

"""Generate the deterministic LTFA MVP benchmark fixtures."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Iterable


PROFILE_ID = "mvp-10k-50k-100k-v1"
ENTITY_COUNT = 10_000
RELATION_COUNT = 50_000
OBSERVATION_COUNT = 100_000
ANALYSIS_AT = "2030-01-02T03:47:00Z"


def write_lines(path: Path, lines: Iterable[str]) -> dict[str, object]:
    temporary = path.with_name(f".{path.name}.tmp")
    digest = hashlib.sha256()
    byte_count = 0
    line_count = 0
    with temporary.open("wb") as output:
        for line in lines:
            payload = f"{line}\n".encode("utf-8")
            output.write(payload)
            digest.update(payload)
            byte_count += len(payload)
            line_count += 1
    os.replace(temporary, path)
    return {
        "bytes": byte_count,
        "lines": line_count,
        "sha256": digest.hexdigest(),
    }


def model_lines() -> Iterable[str]:
    yield "ltfa 1"
    yield (
        'scope site:benchmark kind=site label="Synthetic MVP Benchmark" '
        "completeness=verified evidence-quality=confirmed"
    )
    for index in range(ENTITY_COUNT):
        yield (
            f"entity device:node-{index:05d} kind=device plane=communication "
            "layer=physical scope=site:benchmark"
        )

    relation_count = 0
    for source in range(ENTITY_COUNT):
        for offset in range(1, 6):
            target = source + offset
            if target >= ENTITY_COUNT:
                continue
            yield (
                f"relate device:node-{source:05d} -> device:node-{target:05d} "
                "kind=depends_on"
            )
            relation_count += 1
    for source in range(15):
        yield (
            f"relate device:node-{source:05d} -> device:node-{source + 6:05d} "
            "kind=depends_on"
        )
        relation_count += 1
    if relation_count != RELATION_COUNT:
        raise RuntimeError(f"generated {relation_count} relations, expected {RELATION_COUNT}")

    yield (
        "probe probe:benchmark target=device:node-09999 vantage=device:node-00000 "
        "kind=icmp valid-for=60s"
    )


def observation_lines() -> Iterable[str]:
    first_observation = datetime(2030, 1, 1, tzinfo=timezone.utc)
    for sequence in range(OBSERVATION_COUNT):
        observed_at = first_observation + timedelta(seconds=sequence)
        record = {
            "schema": "ltfa.observation/v1",
            "probe_id": "probe:benchmark",
            "target": "device:node-09999",
            "vantage": "device:node-00000",
            "kind": "icmp",
            "outcome": "pass",
            "observed_at": observed_at.strftime("%Y-%m-%dT%H:%M:%SZ"),
            "valid_for_ms": 60_000,
            "producer_sequence": sequence,
        }
        yield json.dumps(record, separators=(",", ":"), ensure_ascii=True)


def generate(output_root: Path) -> dict[str, object]:
    output_root.mkdir(parents=True, exist_ok=True)
    model_path = output_root / "main.ltfa"
    observations_path = output_root / "observations.jsonl"
    manifest_path = output_root / "manifest.json"

    model = write_lines(model_path, model_lines())
    model.update(
        {
            "entities": ENTITY_COUNT,
            "file": model_path.name,
            "probes": 1,
            "relations": RELATION_COUNT,
        }
    )
    observations = write_lines(observations_path, observation_lines())
    observations.update(
        {
            "analysis_at": ANALYSIS_AT,
            "file": observations_path.name,
            "records": OBSERVATION_COUNT,
        }
    )

    manifest = {
        "schema": "ltfa.benchmark-fixtures/v1",
        "profile": PROFILE_ID,
        "generator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "model": model,
        "observations": observations,
    }
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-root",
        type=Path,
        required=True,
        help="Directory that receives main.ltfa, observations.jsonl, and manifest.json.",
    )
    arguments = parser.parse_args()
    manifest = generate(arguments.output_root.resolve())
    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

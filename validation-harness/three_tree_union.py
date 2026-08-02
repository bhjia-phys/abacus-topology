#!/usr/bin/env python3
"""Truthful three-tree CTest comparison for the semantic-merge audit.

Job 1091 captured real CTest return codes, but its shell classifier treated
``PASS`` on the merge tree as ``PASS_ALL`` without checking either parent and
did not handle merge-side timeouts.  It also hashed non-verbose CTest wrapper
summaries instead of the test output.  This tool deliberately separates:

* discovery evidence imported from job 1091;
* verbose re-runs for every merge-side non-pass and explicit alias mapping;
* deterministic classification and optional, auditable manual overrides.

The final gate passes only when no merge regression, missing merge test, or
unreviewed unknown remains.  Parent failures do not turn a passing merge test
into a failure, but they are never called PASS_ALL.
"""

from __future__ import annotations

import argparse
import csv
import dataclasses
import datetime as dt
import hashlib
import json
import os
import pathlib
import re
import signal
import shutil
import subprocess
import sys
from typing import Dict, Iterable, List, Mapping, MutableMapping, Optional, Sequence, Tuple


ROLES = ("merge", "soc", "master")
NONPASS = {"FAIL", "TIMEOUT", "NOT_RUN"}
BLOCKING_CLASSES = {
    "MERGE_REGRESSION",
    "MERGE_TEST_MISSING",
    "MERGE_NOT_RUN",
    "UNKNOWN",
}
ALLOWED_OVERRIDES = {
    "ENVIRONMENT_REPRODUCED_ALL",
    "INHERITED_SOC",
    "INHERITED_MASTER",
    "INHERITED_BOTH",
    "UPSTREAM_PARENT_DEFECT",
}


@dataclasses.dataclass
class Observation:
    registered: bool
    test_name: str
    rc: str
    status: str
    raw_sha256: str = "NA"
    signature_sha256: str = "NA"
    log: str = "NA"
    source: str = "NA"

    @classmethod
    def not_registered(cls, test_name: str = "NA") -> "Observation":
        return cls(False, test_name, "NOT_REG", "NOT_REG")


@dataclasses.dataclass
class Classified:
    classification: str
    reason: str


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def classify(observations: Mapping[str, Observation]) -> Classified:
    merge = observations["merge"]
    soc = observations["soc"]
    master = observations["master"]
    parents = (("SOC", soc), ("MASTER", master))

    if not merge.registered:
        return Classified("MERGE_TEST_MISSING", "parent test has no merge registration or alias")
    if merge.status == "PASS":
        if soc.status == "PASS" and master.status == "PASS":
            return Classified("PASS_ALL", "all three registered tests returned rc=0")
        details = "; ".join(
            "%s=%s" % (name.lower(), obs.status) for name, obs in parents
        )
        return Classified(
            "MERGE_PASS_PARENT_NONPASS",
            "merge passed; parent baseline is not all-pass (%s)" % details,
        )
    if merge.status == "NOT_RUN":
        return Classified("MERGE_NOT_RUN", "merge test was registered but executable did not run")

    # The preservation contract is conservative: a merge failure is a
    # regression candidate whenever either comparable parent passes, even if
    # the other parent also fails.
    passing = [name for name, obs in parents if obs.status == "PASS"]
    if passing:
        return Classified(
            "MERGE_REGRESSION",
            "merge %s while parent passed: %s" % (merge.status.lower(), ",".join(passing)),
        )

    matching: List[str] = []
    if merge.signature_sha256 not in {"", "NA"}:
        for name, obs in parents:
            if (
                obs.registered
                and obs.status in NONPASS
                and obs.signature_sha256 == merge.signature_sha256
            ):
                matching.append(name)
    if matching == ["SOC", "MASTER"]:
        return Classified("INHERITED_BOTH", "normalized verbose signature matches both parents")
    if matching == ["SOC"]:
        return Classified("INHERITED_SOC", "normalized verbose signature matches SOC parent")
    if matching == ["MASTER"]:
        return Classified("INHERITED_MASTER", "normalized verbose signature matches master parent")

    return Classified(
        "UNKNOWN",
        "merge is non-pass and no passing parent or matching verbose failure signature was proven",
    )


def status_from_run(returncode: int, text: str, timed_out: bool) -> str:
    if timed_out or returncode == 124:
        return "TIMEOUT"
    if "Unable to find executable" in text or "***Not Run" in text:
        return "NOT_RUN"
    if "***Timeout" in text or "Timeout while running" in text:
        return "TIMEOUT"
    if returncode == 0 and re.search(r"100% tests passed, 0 tests failed", text):
        return "PASS"
    return "FAIL"


_VOLATILE_LINES = (
    "UpdateCTestConfiguration",
    "Constructing a list of tests",
    "Done constructing a list of tests",
    "Checking test dependency graph",
    "Checking test dependency graph end",
    "Test project ",
    "Test command:",
    "Working Directory:",
    "Test timeout computed to be:",
)


def normalized_verbose_signature(
    text: str, actual_test: Optional[str] = None, canonical_test: Optional[str] = None
) -> str:
    """Return an order-preserving signature containing actual verbose output.

    CTest prefixes verbose child output with ``<test-number>:``.  We remove
    only that wrapper prefix and narrowly defined volatile metadata.  We do
    not sort lines or normalize arbitrary scientific numbers.
    """

    body: List[str] = []
    for raw in text.splitlines():
        line = raw.rstrip()
        prefixed = re.match(r"^\s*\d+:\s?(.*)$", line)
        if prefixed:
            line = prefixed.group(1)
        if any(line.startswith(prefix) for prefix in _VOLATILE_LINES):
            continue
        if re.match(r"^\s*Start\s+\d+:\s*", line):
            continue
        if re.match(r"^\s*\d+/\d+\s+Test\s+#\d+:", line):
            line = re.sub(r"Test\s+#\d+:", "Test #N:", line)
            line = re.sub(r"\s+[0-9]+(?:\.[0-9]+)?\s+sec\s*$", " N.N sec", line)
        if re.match(r"^\s*\d+\s+-\s+[^ ]+\s+\(", line):
            line = re.sub(r"^\s*\d+\s+-\s+", "N - ", line)
        if re.match(r"^test\s+\d+$", line):
            line = "test N"
        if re.match(r"^\[----------\] Time elapsed: [0-9.]+ seconds$", line):
            line = "[----------] Time elapsed: N.N seconds"
        if re.match(r"^Total Test time \(real\) = [0-9.]+ sec$", line):
            line = "Total Test time (real) = N.N sec"
        if re.match(r"^\s*Commit:\s+", line):
            line = re.sub(r"Commit:.*$", "Commit: COMMIT", line)
        if re.match(
            r"^\s*(?:Sun|Mon|Tue|Wed|Thu|Fri|Sat) [A-Z][a-z]{2}\s+\d+\s+[0-9:]+\s+\d{4}\s*$",
            line,
        ):
            line = "RUN_DATE"

        if actual_test and canonical_test and actual_test != canonical_test:
            line = line.replace(actual_test, canonical_test)
        line = re.sub(
            r"/(?:home/bhj|data/users/bhj)/ai-runs/[^\s:'\"]+/(?:source|build)-(?:merge|soc|master)",
            "<TREE_ROOT>",
            line,
        )
        line = re.sub(
            r"/(?:home/bhj|data/users/bhj)/[^\s:'\"]+", "<REMOTE_PATH>", line
        )
        line = re.sub(r"/home/bhjia/physics/GW_librpa/[^\s:'\"]+", "<LOCAL_PATH>", line)
        line = re.sub(r"\b0x[0-9a-fA-F]+\b", "ADDR", line)
        line = re.sub(r"\bpid[ =:]\s*[0-9]+\b", "pid=PID", line, flags=re.IGNORECASE)
        line = re.sub(
            r"\b[0-9]{4}-[0-9]{2}-[0-9]{2}[T ][0-9:.+-]+\b",
            "TIMESTAMP",
            line,
        )
        if line:
            body.append(line)
    return "\n".join(body).strip() + "\n"


def discover_tests(build: pathlib.Path, evidence_dir: pathlib.Path, role: str) -> List[str]:
    json_path = evidence_dir / ("registration-%s.json" % role)
    proc = subprocess.run(
        ["ctest", "--test-dir", str(build), "-N", "--show-only=json-v1"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    json_path.write_text(proc.stdout, encoding="utf-8")
    if proc.returncode == 0:
        try:
            data = json.loads(proc.stdout)
            tests = [item["name"] for item in data.get("tests", [])]
            if tests:
                return sorted(set(tests))
        except (ValueError, KeyError, TypeError):
            pass

    text_path = evidence_dir / ("registration-%s.txt" % role)
    fallback = subprocess.run(
        ["ctest", "--test-dir", str(build), "-N"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    text_path.write_text(fallback.stdout, encoding="utf-8")
    if fallback.returncode != 0:
        raise RuntimeError("CTest registration failed for %s" % role)
    tests = re.findall(r"Test\s+#\d+:\s+(\S+)", fallback.stdout)
    if not tests:
        raise RuntimeError("CTest registration for %s produced no test names" % role)
    return sorted(set(tests))


def load_aliases(path: pathlib.Path) -> Dict[str, Dict[str, str]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    aliases: Dict[str, Dict[str, str]] = {}
    for canonical, mapping in data.items():
        if set(mapping) != set(ROLES):
            raise ValueError("alias %s must define merge, soc and master" % canonical)
        aliases[canonical] = {role: str(mapping[role]) for role in ROLES}
    return aliases


def canonical_registry(
    registrations: Mapping[str, Sequence[str]],
    aliases: Mapping[str, Mapping[str, str]],
) -> Dict[str, Dict[str, Optional[str]]]:
    reverse: Dict[str, Dict[str, str]] = {role: {} for role in ROLES}
    for canonical, mapping in aliases.items():
        for role, actual in mapping.items():
            if actual in reverse[role]:
                raise ValueError("duplicate %s alias for %s" % (role, actual))
            reverse[role][actual] = canonical

    union: Dict[str, Dict[str, Optional[str]]] = {}
    for role in ROLES:
        for actual in registrations[role]:
            canonical = reverse[role].get(actual, actual)
            union.setdefault(canonical, {name: None for name in ROLES})[role] = actual
    return dict(sorted(union.items()))


def load_seed(path: pathlib.Path) -> Dict[str, Mapping[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    result: Dict[str, Mapping[str, str]] = {}
    for row in rows:
        name = row["test"]
        if name in result:
            raise ValueError("duplicate seed row: %s" % name)
        result[name] = row
    return result


def seed_observation(
    row: Optional[Mapping[str, str]], role: str, actual: Optional[str], canonical: str
) -> Observation:
    if actual is None:
        return Observation.not_registered()
    if row is None or actual != canonical:
        return Observation(True, actual, "UNRUN", "UNRUN", source="needs-verbose-rerun")
    prefix = "master" if role == "master" else role
    status = row["%s_status" % prefix]
    rc = row["%s_rc" % prefix]
    signature = row.get("%s_sig_sha" % prefix, "NA")
    return Observation(
        True,
        actual,
        rc,
        status,
        signature_sha256=signature,
        source="job1091-seed",
    )


def safe_name(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.+-]", "_", value)


def run_command_with_timeout(
    command: Sequence[str], timeout_seconds: float, env: Mapping[str, str]
) -> Tuple[int, str, bool]:
    process = subprocess.Popen(
        list(command),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        env=dict(env),
        start_new_session=True,
    )
    try:
        output, _ = process.communicate(timeout=timeout_seconds)
        return int(process.returncode), output, False
    except subprocess.TimeoutExpired:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            output, _ = process.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            output, _ = process.communicate()
        return 124, output, True


def run_one(
    build: pathlib.Path,
    canonical: str,
    actual: str,
    role: str,
    result_dir: pathlib.Path,
    timeout_seconds: int,
) -> Observation:
    log_dir = result_dir / "verbose-logs" / safe_name(canonical)
    log_dir.mkdir(parents=True, exist_ok=True)
    log_path = log_dir / (role + ".log")
    sig_path = log_dir / (role + ".signature.txt")
    command = [
        "ctest",
        "--test-dir",
        str(build),
        "-R",
        "^%s$" % re.escape(actual),
        "-V",
        "--output-on-failure",
    ]
    returncode, text, timed_out = run_command_with_timeout(
        command,
        timeout_seconds,
        dict(os.environ, OMP_NUM_THREADS="1"),
    )
    if timed_out:
        text += "\nHARNESS_TIMEOUT_SECONDS=%d\n" % timeout_seconds
    log_path.write_text(text, encoding="utf-8")
    signature = normalized_verbose_signature(text, actual, canonical)
    sig_path.write_text(signature, encoding="utf-8")
    return Observation(
        True,
        actual,
        str(returncode),
        status_from_run(returncode, text, timed_out),
        raw_sha256=sha256_file(log_path),
        signature_sha256=sha256_bytes(signature.encode("utf-8")),
        log=str(log_path.relative_to(result_dir)),
        source="verbose-rerun",
    )


def load_overrides(path: Optional[pathlib.Path]) -> Dict[str, Mapping[str, str]]:
    if path is None:
        return {}
    with path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    overrides: Dict[str, Mapping[str, str]] = {}
    for row in rows:
        canonical = row["canonical_test"]
        if canonical in overrides:
            raise ValueError("duplicate override: %s" % canonical)
        if row["classification"] not in ALLOWED_OVERRIDES:
            raise ValueError("disallowed override classification for %s" % canonical)
        if not row.get("reason", "").strip() or not row.get("evidence", "").strip():
            raise ValueError("override requires reason and evidence: %s" % canonical)
        overrides[canonical] = row
    return overrides


def observation_to_dict(obs: Observation) -> Mapping[str, object]:
    return dataclasses.asdict(obs)


def observation_from_dict(data: Mapping[str, object]) -> Observation:
    return Observation(**data)  # type: ignore[arg-type]


def write_manifest(result_dir: pathlib.Path) -> None:
    manifest = result_dir / "MANIFEST.sha256"
    lines: List[str] = []
    for path in sorted(result_dir.rglob("*")):
        if not path.is_file() or path == manifest:
            continue
        lines.append("%s  %s" % (sha256_file(path), path.relative_to(result_dir)))
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")


def verify_manifest(result_dir: pathlib.Path) -> None:
    manifest = result_dir / "MANIFEST.sha256"
    if not manifest.is_file():
        raise RuntimeError("source result has no MANIFEST.sha256")
    for line in manifest.read_text(encoding="utf-8").splitlines():
        expected, separator, relative = line.partition("  ")
        path = pathlib.Path(relative)
        if not separator or path.is_absolute() or ".." in path.parts:
            raise RuntimeError("invalid manifest row: %s" % line)
        target = result_dir / path
        if not target.is_file() or sha256_file(target) != expected:
            raise RuntimeError("manifest verification failed: %s" % relative)


def refresh_verbose_signatures(
    result_dir: pathlib.Path,
    observations_by_test: MutableMapping[str, MutableMapping[str, Observation]],
) -> None:
    for canonical, observations in observations_by_test.items():
        for role, observation in observations.items():
            if observation.log in {"", "NA"}:
                continue
            log_path = result_dir / observation.log
            if not log_path.is_file():
                raise RuntimeError("missing %s log for %s" % (role, canonical))
            raw_sha = sha256_file(log_path)
            if observation.raw_sha256 not in {"", "NA", raw_sha}:
                raise RuntimeError("raw log hash changed for %s/%s" % (canonical, role))
            signature = normalized_verbose_signature(
                log_path.read_text(encoding="utf-8"),
                observation.test_name,
                canonical,
            )
            signature_path = log_path.with_name(role + ".signature.txt")
            signature_path.write_text(signature, encoding="utf-8")
            observation.raw_sha256 = raw_sha
            observation.signature_sha256 = sha256_bytes(signature.encode("utf-8"))


def finalize(
    result_dir: pathlib.Path,
    observations_by_test: Mapping[str, Mapping[str, Observation]],
    overrides: Mapping[str, Mapping[str, str]],
    merge_commit: str,
) -> int:
    table_path = result_dir / "union-table-final.csv"
    fieldnames = ["canonical_test"]
    for role in ROLES:
        fieldnames.extend(
            [
                "%s_test" % role,
                "%s_status" % role,
                "%s_rc" % role,
                "%s_raw_sha256" % role,
                "%s_signature_sha256" % role,
                "%s_log" % role,
                "%s_source" % role,
            ]
        )
    fieldnames.extend(["classification", "reason", "override_evidence"])

    counts: MutableMapping[str, int] = {}
    blocking: List[str] = []
    with table_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for canonical in sorted(observations_by_test):
            observations = observations_by_test[canonical]
            decision = classify(observations)
            override_evidence = ""
            if canonical in overrides:
                if decision.classification != "UNKNOWN":
                    raise ValueError("override applies only to UNKNOWN: %s" % canonical)
                override = overrides[canonical]
                decision = Classified(override["classification"], override["reason"])
                override_evidence = override["evidence"]
            counts[decision.classification] = counts.get(decision.classification, 0) + 1
            if decision.classification in BLOCKING_CLASSES:
                blocking.append(canonical)
            row: Dict[str, object] = {"canonical_test": canonical}
            for role in ROLES:
                obs = observations[role]
                row.update(
                    {
                        "%s_test" % role: obs.test_name,
                        "%s_status" % role: obs.status,
                        "%s_rc" % role: obs.rc,
                        "%s_raw_sha256" % role: obs.raw_sha256,
                        "%s_signature_sha256" % role: obs.signature_sha256,
                        "%s_log" % role: obs.log,
                        "%s_source" % role: obs.source,
                    }
                )
            row.update(
                {
                    "classification": decision.classification,
                    "reason": decision.reason,
                    "override_evidence": override_evidence,
                }
            )
            writer.writerow(row)

    summary = {
        "merge_commit": merge_commit,
        "generated_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "total": len(observations_by_test),
        "counts": dict(sorted(counts.items())),
        "blocking_tests": blocking,
        "gate": "PASS" if not blocking else "FAIL",
    }
    (result_dir / "summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    marker = result_dir / ("PASS" if not blocking else "FAIL")
    marker.write_text(
        merge_commit + "\n" + ("no blocking classifications\n" if not blocking else "\n".join(blocking) + "\n"),
        encoding="utf-8",
    )
    write_manifest(result_dir)
    return 0 if not blocking else 1


def command_run(args: argparse.Namespace) -> int:
    result_dir = pathlib.Path(args.result_dir).resolve()
    if result_dir.exists():
        raise RuntimeError("immutable result directory already exists: %s" % result_dir)
    result_dir.mkdir(parents=True)
    builds = {role: pathlib.Path(getattr(args, "%s_build" % role)).resolve() for role in ROLES}
    for role, build in builds.items():
        if not build.is_dir():
            raise RuntimeError("%s build directory missing: %s" % (role, build))

    registrations = {
        role: discover_tests(builds[role], result_dir, role) for role in ROLES
    }
    for role, names in registrations.items():
        (result_dir / ("registered-%s.txt" % role)).write_text(
            "\n".join(names) + "\n", encoding="utf-8"
        )
    aliases = load_aliases(pathlib.Path(args.aliases))
    registry = canonical_registry(registrations, aliases)
    seed = load_seed(pathlib.Path(args.seed_table))
    overrides = load_overrides(pathlib.Path(args.overrides) if args.overrides else None)

    observations_by_test: Dict[str, Dict[str, Observation]] = {}
    selected: List[str] = []
    for canonical, role_names in registry.items():
        row = seed.get(canonical)
        observations = {
            role: seed_observation(row, role, role_names[role], canonical) for role in ROLES
        }
        merge_nonpass = observations["merge"].status != "PASS"
        alias_mapped = canonical in aliases
        missing_seed = row is None
        if merge_nonpass or alias_mapped or missing_seed:
            selected.append(canonical)
            for role in ROLES:
                actual = role_names[role]
                observations[role] = (
                    run_one(
                        builds[role],
                        canonical,
                        actual,
                        role,
                        result_dir,
                        args.timeout,
                    )
                    if actual is not None
                    else Observation.not_registered()
                )
        observations_by_test[canonical] = observations

    (result_dir / "rerun-tests.txt").write_text(
        "\n".join(selected) + "\n", encoding="utf-8"
    )
    serializable = {
        canonical: {role: observation_to_dict(obs) for role, obs in observations.items()}
        for canonical, observations in observations_by_test.items()
    }
    (result_dir / "observations.json").write_text(
        json.dumps(serializable, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (result_dir / "provenance.json").write_text(
        json.dumps(
            {
                "merge_commit": args.merge_commit,
                "soc_commit": args.soc_commit,
                "master_commit": args.master_commit,
                "builds": {role: str(path) for role, path in builds.items()},
                "seed_table": str(pathlib.Path(args.seed_table).resolve()),
                "seed_table_sha256": sha256_file(pathlib.Path(args.seed_table)),
                "aliases": str(pathlib.Path(args.aliases).resolve()),
                "aliases_sha256": sha256_file(pathlib.Path(args.aliases)),
                "harness_sha256": sha256_file(pathlib.Path(__file__).resolve()),
                "timeout_seconds": args.timeout,
                "rerun_count": len(selected),
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    return finalize(result_dir, observations_by_test, overrides, args.merge_commit)


def command_finalize(args: argparse.Namespace) -> int:
    result_dir = pathlib.Path(args.result_dir).resolve()
    if result_dir.exists():
        raise RuntimeError("immutable result directory already exists: %s" % result_dir)
    result_dir.mkdir(parents=True)
    data = json.loads(pathlib.Path(args.observations).read_text(encoding="utf-8"))
    observations_by_test = {
        canonical: {
            role: observation_from_dict(role_data) for role, role_data in observations.items()
        }
        for canonical, observations in data.items()
    }
    overrides = load_overrides(pathlib.Path(args.overrides) if args.overrides else None)
    return finalize(result_dir, observations_by_test, overrides, args.merge_commit)


def command_reclassify(args: argparse.Namespace) -> int:
    source_dir = pathlib.Path(args.source_result).resolve()
    result_dir = pathlib.Path(args.result_dir).resolve()
    if not source_dir.is_dir():
        raise RuntimeError("source result directory missing: %s" % source_dir)
    if result_dir.exists():
        raise RuntimeError("immutable result directory already exists: %s" % result_dir)
    if source_dir == result_dir or source_dir in result_dir.parents:
        raise RuntimeError("reclassified result must not be nested inside source result")
    verify_manifest(source_dir)
    source_manifest_sha256 = sha256_file(source_dir / "MANIFEST.sha256")
    shutil.copytree(source_dir, result_dir)
    for name in ("PASS", "FAIL", "MANIFEST.sha256", "summary.json", "union-table-final.csv"):
        path = result_dir / name
        if path.exists():
            path.unlink()

    data = json.loads((result_dir / "observations.json").read_text(encoding="utf-8"))
    observations_by_test: Dict[str, Dict[str, Observation]] = {
        canonical: {
            role: observation_from_dict(role_data) for role, role_data in observations.items()
        }
        for canonical, observations in data.items()
    }
    refresh_verbose_signatures(result_dir, observations_by_test)
    serializable = {
        canonical: {role: observation_to_dict(obs) for role, obs in observations.items()}
        for canonical, observations in observations_by_test.items()
    }
    (result_dir / "observations.json").write_text(
        json.dumps(serializable, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    provenance_path = result_dir / "provenance.json"
    provenance = json.loads(provenance_path.read_text(encoding="utf-8"))
    provenance.update(
        {
            "reclassified_from": str(source_dir),
            "source_manifest_sha256": source_manifest_sha256,
            "harness_sha256": sha256_file(pathlib.Path(__file__).resolve()),
        }
    )
    provenance_path.write_text(
        json.dumps(provenance, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    overrides = load_overrides(pathlib.Path(args.overrides) if args.overrides else None)
    return finalize(result_dir, observations_by_test, overrides, args.merge_commit)


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description=__doc__)
    subparsers = root.add_subparsers(dest="command", required=True)
    run = subparsers.add_parser("run", help="discover, selectively rerun and classify")
    for role in ROLES:
        run.add_argument("--%s-build" % role, required=True)
        run.add_argument("--%s-commit" % role, required=True)
    run.add_argument("--seed-table", required=True)
    run.add_argument("--aliases", required=True)
    run.add_argument("--overrides")
    run.add_argument("--result-dir", required=True)
    run.add_argument("--timeout", type=int, default=900)
    run.set_defaults(func=command_run)

    final = subparsers.add_parser("finalize", help="reclassify saved observations")
    final.add_argument("--observations", required=True)
    final.add_argument("--overrides")
    final.add_argument("--merge-commit", required=True)
    final.add_argument("--result-dir", required=True)
    final.set_defaults(func=command_finalize)

    reclassify = subparsers.add_parser(
        "reclassify", help="verify and copy a raw result, then refresh verbose signatures"
    )
    reclassify.add_argument("--source-result", required=True)
    reclassify.add_argument("--overrides")
    reclassify.add_argument("--merge-commit", required=True)
    reclassify.add_argument("--result-dir", required=True)
    reclassify.set_defaults(func=command_reclassify)
    return root


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parser().parse_args(argv)
    try:
        return int(args.func(args))
    except Exception as exc:  # concise top-level failure for Slurm evidence
        print("HARNESS_ERROR: %s" % exc, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3

import importlib.util
import csv
import json
import os
import pathlib
import sys
import tempfile
import types
import unittest


MODULE_PATH = pathlib.Path(__file__).with_name("three_tree_union.py")
SPEC = importlib.util.spec_from_file_location("three_tree_union", MODULE_PATH)
assert SPEC and SPEC.loader
TTU = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = TTU
SPEC.loader.exec_module(TTU)


def obs(status, signature="NA", registered=True):
    return TTU.Observation(registered, "test", "0" if status == "PASS" else "8", status,
                           signature_sha256=signature)


class ClassificationTest(unittest.TestCase):
    def test_pass_all_requires_both_parents(self):
        result = TTU.classify({"merge": obs("PASS"), "soc": obs("PASS"), "master": obs("FAIL")})
        self.assertEqual("MERGE_PASS_PARENT_NONPASS", result.classification)

    def test_all_pass(self):
        result = TTU.classify({role: obs("PASS") for role in TTU.ROLES})
        self.assertEqual("PASS_ALL", result.classification)

    def test_timeout_is_not_silently_passed(self):
        result = TTU.classify({"merge": obs("TIMEOUT", "same"),
                               "soc": obs("TIMEOUT", "same"),
                               "master": TTU.Observation.not_registered()})
        self.assertEqual("INHERITED_SOC", result.classification)

    def test_parent_pass_has_regression_precedence(self):
        result = TTU.classify({"merge": obs("FAIL", "same"),
                               "soc": obs("FAIL", "same"),
                               "master": obs("PASS")})
        self.assertEqual("MERGE_REGRESSION", result.classification)

    def test_unknown_without_detailed_signature(self):
        result = TTU.classify({"merge": obs("FAIL"), "soc": obs("FAIL"), "master": obs("FAIL")})
        self.assertEqual("UNKNOWN", result.classification)

    def test_missing_merge_registration_blocks(self):
        result = TTU.classify({"merge": TTU.Observation.not_registered(),
                               "soc": obs("PASS"), "master": obs("PASS")})
        self.assertEqual("MERGE_TEST_MISSING", result.classification)

    def test_not_run_merge_blocks(self):
        result = TTU.classify({"merge": obs("NOT_RUN"), "soc": obs("FAIL"), "master": obs("FAIL")})
        self.assertEqual("MERGE_NOT_RUN", result.classification)


class SignatureTest(unittest.TestCase):
    def test_wrapper_numbers_and_paths_are_removed_but_order_is_preserved(self):
        text = """Test project /data/users/bhj/run/build
    Start 303: demo
303: Test command: /data/users/bhj/run/build/demo
303: Working Directory: /data/users/bhj/run/build
303: [ RUN      ] Demo.Value
303: source = /data/users/bhj/ai-runs/run-id/source-merge/source/demo.cpp:14
303: scientific value = 1.2345
303: pid 4242 at 0xabc123
1/1 Test #303: demo ***Failed  2.31 sec
 303 - demo (Failed)
"""
        normalized = TTU.normalized_verbose_signature(text)
        self.assertIn("scientific value = 1.2345", normalized)
        self.assertIn("pid=PID at ADDR", normalized)
        self.assertNotIn("/data/users", normalized)
        self.assertNotIn("#303", normalized)
        self.assertLess(normalized.index("[ RUN"), normalized.index("scientific value"))

    def test_tree_role_paths_and_alias_names_normalize_identically(self):
        merge = """42: /data/users/bhj/ai-runs/run/source-merge/source/demo.cpp: failure
1/1 Test #42: MODULE_CELL_magnetism ***Failed  1.0 sec
"""
        master = """17: /data/users/bhj/ai-runs/run/source-master/source/demo.cpp: failure
1/1 Test #17: MODULE_ESTATE_elecstate_magnetism ***Failed  2.0 sec
"""
        merge_sig = TTU.normalized_verbose_signature(
            merge, "MODULE_CELL_magnetism", "MODULE_CELL_magnetism"
        )
        master_sig = TTU.normalized_verbose_signature(
            master, "MODULE_ESTATE_elecstate_magnetism", "MODULE_CELL_magnetism"
        )
        self.assertEqual(merge_sig, master_sig)
        self.assertIn("<TREE_ROOT>/source/demo.cpp", merge_sig)

    def test_ctest_and_abacus_timing_metadata_do_not_change_signature(self):
        merge = """test 303
[----------] Time elapsed: 1.899 seconds
                      Commit: 7ef8506a8 (Sun Aug 2 20:53:33 2026 +0800)
 Sun Aug  2 21:09:14 2026
scientific value = 1.2345
Total Test time (real) = 294.69 sec
"""
        soc = """test 300
[----------] Time elapsed: 1.853 seconds
                      Commit: 4aa46ed65 (Thu Jul 30 03:18:41 2026 -0400)
 Sun Aug  2 21:14:09 2026
scientific value = 1.2345
Total Test time (real) = 295.14 sec
"""
        self.assertEqual(
            TTU.normalized_verbose_signature(merge),
            TTU.normalized_verbose_signature(soc),
        )
        self.assertIn(
            "scientific value = 1.2345", TTU.normalized_verbose_signature(merge)
        )


class Job1091ReclassificationTest(unittest.TestCase):
    def test_real_seed_exposes_fifth_unknown_and_strict_pass_all(self):
        seed = pathlib.Path(
            os.environ.get(
                "JOB1091_SEED_TABLE",
                str(
                    MODULE_PATH.parent.parent
                    / "validation-evidence"
                    / "three-tree-1091"
                    / "union-table.csv"
                ),
            )
        )
        counts = {}
        with seed.open(newline="", encoding="utf-8") as handle:
            for row in csv.DictReader(handle):
                observations = {}
                for role in TTU.ROLES:
                    registered = row["%s_reg" % role] == "1"
                    observations[role] = (
                        TTU.Observation(
                            registered,
                            row["test"],
                            row["%s_rc" % role],
                            row["%s_status" % role],
                            signature_sha256=row["%s_sig_sha" % role],
                        )
                        if registered
                        else TTU.Observation.not_registered()
                    )
                classification = TTU.classify(observations).classification
                counts[classification] = counts.get(classification, 0) + 1
        self.assertEqual(
            {
                "PASS_ALL": 201,
                "MERGE_PASS_PARENT_NONPASS": 61,
                "INHERITED_SOC": 46,
                "UNKNOWN": 5,
            },
            counts,
        )


class ImmutableReclassifyTest(unittest.TestCase):
    def test_verified_raw_logs_can_be_reclassified_without_mutating_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            source = root / "source-result"
            destination = root / "reclassified-result"
            log_dir = source / "verbose-logs" / "demo"
            log_dir.mkdir(parents=True)
            observations = {}
            for number, role, elapsed in (
                (303, "merge", "1.899"),
                (300, "soc", "1.853"),
                (42, "master", "1.712"),
            ):
                log = log_dir / (role + ".log")
                log.write_text(
                    "test %d\n[----------] Time elapsed: %s seconds\nphysics failure = 4.2\n"
                    % (number, elapsed),
                    encoding="utf-8",
                )
                observations[role] = TTU.Observation(
                    True,
                    "demo",
                    "8",
                    "FAIL",
                    raw_sha256=TTU.sha256_file(log),
                    signature_sha256="old-" + role,
                    log=str(log.relative_to(source)),
                    source="verbose-rerun",
                )
            by_test = {"demo": observations}
            (source / "observations.json").write_text(
                json.dumps(
                    {
                        "demo": {
                            role: TTU.observation_to_dict(observation)
                            for role, observation in observations.items()
                        }
                    }
                ),
                encoding="utf-8",
            )
            (source / "provenance.json").write_text("{}\n", encoding="utf-8")
            self.assertEqual(1, TTU.finalize(source, by_test, {}, "merge-commit"))
            source_manifest = TTU.sha256_file(source / "MANIFEST.sha256")

            args = types.SimpleNamespace(
                source_result=str(source),
                result_dir=str(destination),
                merge_commit="merge-commit",
                overrides=None,
            )
            self.assertEqual(0, TTU.command_reclassify(args))
            self.assertTrue((source / "FAIL").is_file())
            self.assertEqual(source_manifest, TTU.sha256_file(source / "MANIFEST.sha256"))
            self.assertTrue((destination / "PASS").is_file())
            summary = json.loads((destination / "summary.json").read_text())
            self.assertEqual({"INHERITED_BOTH": 1}, summary["counts"])
            TTU.verify_manifest(destination)


class ProcessBoundaryTest(unittest.TestCase):
    def test_timeout_terminates_isolated_process_group(self):
        rc, output, timed_out = TTU.run_command_with_timeout(
            [
                sys.executable,
                "-c",
                "import subprocess,time; subprocess.Popen(['sleep','60']); print('started', flush=True); time.sleep(60)",
            ],
            0.1,
            os.environ,
        )
        self.assertEqual(124, rc)
        self.assertTrue(timed_out)
        self.assertIn("started", output)


class ForceRerunTest(unittest.TestCase):
    def test_force_rerun_file_is_trimmed_and_deduplicated(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = pathlib.Path(temporary) / "force.txt"
            path.write_text("beta # reason\n\nalpha\n", encoding="utf-8")
            self.assertEqual(["alpha", "beta"], TTU.load_force_reruns(path))
            path.write_text("alpha\nalpha\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate"):
                TTU.load_force_reruns(path)


if __name__ == "__main__":
    unittest.main()

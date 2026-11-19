#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""
Tests for tools/check_sensor_modes.py. Standard library only:

    python3 tools/tests/test_check_sensor_modes.py

The fixture tree under sensor-modes/ has the layout of the SDK root and is
never built. It holds frozen copies of four drivers, so that a later fix of
the real driver does not change what is tested here:

  4.4.94/sensor-src/t31/gc2053.c   as of 01fc6853, before pull request 13:
                                   the 30 fps MIPI table programs 1181 lines,
                                   the mode reports 1418
  4.4.94/sensor-src/t32/gc13b0.c   as of 1ecb6041: frame-length bytes in the
                                   opposite order in the table and set_fps()
  3.10.14/sensor-src/t20/sp1409.c  as of 1ecb6041: set_fps() reads registers
                                   on a page the init table does not write
  3.10.14/sensor-src/t31/jxf32.c   as of 1ecb6041: a mode that takes the
                                   attributes of another window

with each SoC's tx-isp-common.h (the only header copied), and four small
files written for the tests (fx_*.c; each says in its header what it is
for).

expected.json is the script's whole JSON output for that tree. After a
change that is meant to alter the output, rewrite it and read the diff:

    UPDATE_EXPECTED=1 python3 tools/tests/test_check_sensor_modes.py
"""

import contextlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.dont_write_bytecode = True      # no __pycache__ beside the script

import check_sensor_modes as csm      # noqa: E402

TREE = os.path.join(HERE, "sensor-modes")
EXPECTED = os.path.join(TREE, "expected.json")

GC2053 = "4.4.94/sensor-src/t31/gc2053.c"
GC13B0 = "4.4.94/sensor-src/t32/gc13b0.c"
SP1409 = "3.10.14/sensor-src/t20/sp1409.c"
JXF32 = "3.10.14/sensor-src/t31/jxf32.c"
FX = "4.4.94/sensor-src/t31/fx_%s.c"


def run(*args):
    """Run the script on the fixture tree: (exit code, stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        try:
            code = csm.main(["--root", TREE] + list(args))
        except SystemExit as e:     # argparse
            code = e.code
    return code, out.getvalue(), err.getvalue()


def run_json(*args):
    code, out, err = run("--json", *args)
    return code, json.loads(out)


class FixtureTree(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.code, cls.data = run_json("--min-severity", "low")
        cls.drivers = dict((d["file"], d) for d in cls.data["drivers"])

    def modes(self, rel):
        return dict((m["mode"], m) for m in self.drivers[rel]["modes"])

    def findings(self, rel, check=None):
        return [f for f in self.data["findings"]
                if f["file"] == rel and (check is None or f["check"] == check)]

    def test_whole_tree_fails_for_the_file_that_does_not_parse(self):
        # fx_broken is in the tree; every other result is still reported
        self.assertEqual(self.code, 1)
        self.assertEqual(self.data["version"], csm.VERSION)
        self.assertEqual(self.data["summary"]["analysed"], self.data["summary"]["drivers"] - 1)

    def test_expected_output(self):
        got = dict(self.data)
        got.pop("tree")             # the commit of whatever checkout holds the fixtures
        text = json.dumps(got, indent=1, sort_keys=True, default=str) + "\n"
        if os.environ.get("UPDATE_EXPECTED"):
            with open(EXPECTED, "w", encoding="utf-8", newline="\n") as fh:
                fh.write(text)
        with open(EXPECTED, "r", encoding="utf-8") as fh:
            want = fh.read()
        self.assertEqual(text, want, "output differs from expected.json "
                         "(UPDATE_EXPECTED=1 rewrites it; read the diff)")

    def test_gc2053_table_against_total_height(self):
        m = self.modes(GC2053)["sensor_max_fps=30,data_interface=MIPI"]
        self.assertEqual((m["T"], m["total_height"], m["C"]), (1181, 1418, 1181))
        f = self.findings(GC2053, "frame-length")
        self.assertEqual([(x["severity"], x["mode"]) for x in f],
                         [("high", "sensor_max_fps=30,data_interface=MIPI")])
        # the modes that agree stay quiet
        m = self.modes(GC2053)["sensor_max_fps=15,data_interface=MIPI"]
        self.assertEqual((m["T"], m["total_height"], m["C"]), (1181, 1181, 1181))

    def test_gc13b0_byte_order_reports_the_table_frame(self):
        f = self.findings(GC13B0, "byte-order")
        self.assertEqual([x["severity"] for x in f], ["high"])
        m = self.modes(GC13B0)["-"]
        # T is the frame the table holds, not the table's bytes decoded in
        # set_fps()'s order, and it agrees with the finding's own text
        self.assertEqual(m["T"], 1696)
        self.assertEqual(m["T_as_set_fps_reads"], 40966)
        self.assertIn("1696 lines read the table's way", f[0]["message"])

    def test_sp1409_page_register_read(self):
        # set_fps() reads the page register, finds page 1 and reads hts
        # there; the init table writes hts on page 0 only, so the value is
        # not known. It used to be taken from page 0 (hts 2).
        m = self.modes(SP1409)["-"]
        self.assertIsNone(m["C"])
        self.assertIsNone(m["hts"])
        self.assertEqual(self.findings(SP1409), [])

    def test_jxf32_mode_with_another_windows_attributes(self):
        m = self.modes(JXF32)["sensor_resolution=RES_30"]
        self.assertEqual((m["T"], m["total_height"], m["C"]), (1133, 400, 1200))
        checks = sorted(set(f["check"] for f in self.findings(JXF32)
                            if f["mode"] == "sensor_resolution=RES_30" and f["severity"] != "low"))
        self.assertEqual(checks, ["frame-length", "frame-short", "image-size"])

    def test_goto_is_followed(self):
        d = self.drivers[FX % "goto"]
        self.assertEqual(d["status"], "ok")
        m = self.modes(FX % "goto")["-"]
        self.assertEqual((m["T"], m["total_height"], m["C"]), (1200, 1500, 1200))
        self.assertEqual([f["severity"] for f in self.findings(FX % "goto", "frame-length")], ["high"])
        self.assertEqual(d["left_out"]["goto"], 0)

    def test_default_branch(self):
        d = self.drivers[FX % "default"]
        got = sorted((m["mode"], m["T"], m["total_height"], m["C"]) for m in d["modes"])
        self.assertEqual(got, [
            ("sensor_max_fps=25,data_interface=MIPI", 2400, 2400, 2400),   # the default: branch
            ("sensor_max_fps=30,data_interface=MIPI", 1200, 1200, 1200),
        ])
        # DVP reaches a default: that only logs: refused, and said so
        self.assertEqual(d["left_out"]["refused"], 2)
        self.assertTrue(any("refuses" in n for n in d["notes"]))
        self.assertEqual(self.findings(FX % "default"), [])

    def test_preprocessor_conditions(self):
        d = self.drivers[FX % "ppif"]
        m = self.modes(FX % "ppif")["-"]
        self.assertEqual((m["T"], m["total_height"], m["C"]), (1200, 1200, 1200))
        self.assertEqual(m["max_integration_time"], 1196)
        self.assertTrue(any("FX_VERSION" in n and "not read" in n for n in d["notes"]), d["notes"])
        self.assertEqual([(f["severity"], f["check"]) for f in self.findings(FX % "ppif")],
                         [("low", "table-name")])

    def test_file_that_does_not_parse(self):
        d = self.drivers[FX % "broken"]
        self.assertEqual(d["status"], "error")
        self.assertIn("line 21", d["reason"])
        self.assertEqual(self.data["summary"]["errors"], 1)

    def test_left_out_combinations_are_listed(self):
        d = self.drivers[GC13B0]
        self.assertEqual(d["left_out"]["interface-table"], len(d["skipped_combinations"]))
        self.assertGreater(len(d["skipped_combinations"]), 0)
        self.assertEqual(self.data["summary"]["combinations_left_out"]["interface-table"],
                         sum(x["left_out"].get("interface-table", 0) for x in self.data["drivers"]))


class CommandLine(unittest.TestCase):
    def test_fail_on_counts_what_min_severity_hides(self):
        # fx_ppif has one finding, a low one; the default listing starts at medium
        code, out, _ = run("--driver", "fx_ppif", "--fail-on", "low")
        self.assertEqual(code, 1)
        self.assertNotIn("LOW ", out)
        self.assertEqual(run("--driver", "fx_ppif", "--fail-on", "medium")[0], 0)
        self.assertEqual(run("--driver", "fx_ppif")[0], 0)

    def test_a_file_that_does_not_parse_fails_every_run(self):
        for args in ((), ("--fail-on", "high"), ("--min-severity", "low"), ("--strict",)):
            code, out, err = run("--driver", "fx_broken", *args)
            self.assertEqual(code, 1, args)
            self.assertIn("ERROR  not-analysed", out)
            self.assertEqual(err, "failed: 1 driver could not be analysed\n")
        # the JSON output is whole all the same
        code, data = run_json("--driver", "fx_broken", "--driver", "gc2053")
        self.assertEqual(code, 1)
        self.assertEqual(data["summary"]["errors"], 1)
        self.assertEqual(len(data["findings"]), 2)
        # and a run that does not include the file is clean
        self.assertEqual(run("--driver", "gc2053")[0::2], (0, ""))

    def test_stderr_says_what_failed(self):
        code, _, err = run("--driver", "gc2053", "--driver", "fx_broken", "--fail-on", "medium")
        self.assertEqual(code, 1)
        self.assertEqual(err, "failed: 1 driver could not be analysed; "
                              "2 findings of severity medium or worse (--fail-on)\n")
        code, _, err = run("--driver", "fx_ppif", "--fail-on", "low")
        self.assertEqual(err, "failed: 1 finding of severity low or worse (--fail-on)\n")

    def test_unknown_check_name_is_refused(self):
        code, _, err = run("--driver", "gc2053", "--check", "byteorder")
        self.assertEqual(code, 2)
        self.assertIn("byteorder", err)
        self.assertEqual(run("--driver", "gc2053", "--check", "byte-order")[0], 0)

    def test_no_driver_matches(self):
        self.assertEqual(run("--driver", "no_such_sensor")[0], 2)

    def test_modes_table_has_a_header_for_every_column(self):
        _, out, _ = run("--driver", "gc13b0", "--modes")
        lines = out.splitlines()
        head = next(l for l in lines if l.startswith("file "))
        self.assertEqual(head.split(), ["file", "win", "mode", "fps", "T", "A", "C", "width", "note"])
        row = next(l for l in lines if l.startswith(GC13B0))
        self.assertEqual(row.split()[4:7], ["1696", "1696", "1696"])
        self.assertIn("40966", row)

    def test_notes(self):
        _, out, _ = run("--driver", "fx_default", "--notes")
        self.assertIn("NOTE   " + FX % "default", out)
        self.assertNotIn("NOTE", run("--driver", "fx_default")[1])

    def test_baseline(self):
        sel = ["--driver", "gc2053", "--driver", "gc13b0", "--driver", "jxf32"]
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "baseline.json")
            code, _, _ = run("--write-baseline", path, *sel)
            self.assertEqual(code, 0)
            with open(path, "r", encoding="utf-8") as fh:
                base = json.load(fh)
            self.assertEqual(base["min_severity"], "medium")
            self.assertEqual(len(base["findings"]), 7)

            # everything known: nothing listed, nothing to fail on
            code, data = run_json("--baseline", path, "--fail-on", "medium", *sel)
            self.assertEqual(code, 0)
            self.assertEqual(data["findings"], [])
            self.assertEqual((data["baseline"]["known"], data["baseline"]["new"]), (7, 0))
            self.assertEqual(data["baseline"]["gone"], [])
            # the summary still counts every finding
            self.assertEqual(sum(v["high"] for v in data["summary"]["findings"].values()), 5)

            # a baseline of medium and above does not cover the low findings
            self.assertEqual(run("--baseline", path, "--fail-on", "low", *sel)[0], 1)

            # one entry less: that finding is new, and fails the run
            dropped = base["findings"].pop(0)
            with open(path, "w", encoding="utf-8") as fh:
                json.dump(base, fh)
            code, data = run_json("--baseline", path, "--fail-on", "medium", *sel)
            self.assertEqual(code, 1)
            self.assertEqual([(f["file"], f["check"], f["mode"]) for f in data["findings"]],
                             [(dropped["file"], dropped["check"], dropped["mode"])])

            # one entry too many: reported as gone, and does not fail
            base["findings"].append(dropped)
            base["findings"].append({"file": GC2053, "check": "frame-short",
                                     "mode": "sensor_max_fps=15,data_interface=MIPI", "severity": "high"})
            with open(path, "w", encoding="utf-8") as fh:
                json.dump(base, fh)
            code, data = run_json("--baseline", path, "--fail-on", "medium", *sel)
            self.assertEqual(code, 0)
            self.assertEqual([g["check"] for g in data["baseline"]["gone"]], ["frame-short"])

            # a file that does not parse fails the run whatever the baseline says
            self.assertEqual(run("--baseline", path, "--fail-on", "high")[0], 1)

    def test_unreadable_baseline(self):
        code, _, err = run("--driver", "gc2053", "--baseline", os.path.join(TREE, "no-such-file.json"))
        self.assertEqual(code, 2)
        self.assertIn("cannot read baseline", err)

    def test_write_baseline_keeps_the_entries_of_a_file_not_analysed(self):
        broken = FX % "broken"

        def load(path):
            with open(path, "r", encoding="utf-8") as fh:
                return json.load(fh)

        def dump(path, data):
            with open(path, "w", encoding="utf-8") as fh:
                json.dump(data, fh)

        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "baseline.json")
            # every driver analysed: the list is there, and empty
            code, _, err = run("--write-baseline", path, "--driver", "gc2053")
            self.assertEqual((code, err), (0, ""))
            self.assertEqual(load(path)["not_analysed"], [])
            os.remove(path)

            # no earlier baseline: the file is named, and the run says so. The
            # baseline is written, and the run fails for the file all the same
            code, _, err = run("--write-baseline", path, "--driver", "fx_broken")
            self.assertEqual(code, 1)
            self.assertIn("1 driver not analysed (%s)" % broken, err)
            self.assertIn("no earlier baseline", err)
            base = load(path)
            self.assertEqual((base["not_analysed"], base["findings"]), ([broken], []))

            # the file being replaced has entries for it: those within what this
            # run lists are kept, and nothing else is taken from the old file
            kept = {"file": broken, "check": "frame-short", "mode": "m", "severity": "high"}
            too_low = dict(kept, check="table-name", severity="low")
            analysed = {"file": GC2053, "check": "frame-short", "mode": "no such mode", "severity": "high"}
            base["findings"] = [kept, too_low, analysed]
            dump(path, base)
            code, _, err = run("--write-baseline", path, "--driver", "fx_broken", "--driver", "gc2053")
            self.assertEqual(code, 1)
            self.assertIn("1 of their entries kept from", err)
            base = load(path)
            self.assertEqual(base["not_analysed"], [broken])
            self.assertIn(kept, base["findings"])
            self.assertNotIn(too_low, base["findings"])
            self.assertNotIn(analysed, base["findings"])
            self.assertEqual(len(base["findings"]), 3)      # and the two of gc2053
            # written twice, the same file
            run("--write-baseline", path, "--driver", "fx_broken", "--driver", "gc2053")
            self.assertEqual(load(path), base)
            # a run limited to other checks does not carry the entry over
            other = os.path.join(tmp, "other.json")
            run("--baseline", path, "--write-baseline", other, "--driver", "fx_broken",
                "--check", "byte-order")
            self.assertEqual(load(other)["findings"], [])

            # --baseline names the earlier file when another one is written
            run("--baseline", path, "--write-baseline", other, "--driver", "fx_broken")
            self.assertEqual(load(other)["findings"], [kept])

            # with the kept entry the file still fails a gate: it is not analysed
            self.assertEqual(run("--baseline", path, "--fail-on", "high", "--driver", "fx_broken")[0], 1)

            # an earlier baseline that cannot be read, and a file not analysed:
            # nothing is written
            with open(path, "w", encoding="utf-8") as fh:
                fh.write("not json")
            code, _, err = run("--write-baseline", path, "--driver", "fx_broken")
            self.assertEqual(code, 2)
            self.assertIn("is not written", err)
            with open(path, "r", encoding="utf-8") as fh:
                self.assertEqual(fh.read(), "not json")
            # with every driver analysed there is nothing to keep, and it is replaced
            self.assertEqual(run("--write-baseline", path, "--driver", "gc2053")[0], 0)
            self.assertEqual(load(path)["not_analysed"], [])


class Strict(unittest.TestCase):
    """A driver in which no mode is found: counted always, fatal on request."""

    def run_root(self, root, *args):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = csm.main(["--root", root] + list(args))
        return code, out.getvalue(), err.getvalue()

    def test_driver_without_modes(self):
        with tempfile.TemporaryDirectory() as root:
            for rel in (GC2053, "4.4.94/isp/t31/include/tx-isp-common.h"):
                os.makedirs(os.path.dirname(os.path.join(root, rel)), exist_ok=True)
                shutil.copyfile(os.path.join(TREE, rel), os.path.join(root, rel))
            empty = "4.4.94/sensor-src/t31/nomodes.c"
            with open(os.path.join(root, empty), "w", encoding="utf-8", newline="\n") as fh:
                fh.write("/* parses, and describes no sensor mode */\nstatic int unused;\n")

            # counted, not listed and not fatal
            code, out, err = self.run_root(root)
            self.assertEqual((code, err), (0, ""))
            self.assertIn("2 files (2 distinct), 1 analysed, 1 without resolvable modes, 0 not parsed", out)
            self.assertNotIn("NO-MODES", out)
            # nor with --fail-on: it is not a finding
            self.assertEqual(self.run_root(root, "--driver", "nomodes", "--fail-on", "low")[0], 0)

            # --strict: listed with its reason, and fatal
            code, out, err = self.run_root(root, "--strict")
            self.assertEqual(code, 1)
            self.assertIn("NO-MODES  %s: " % empty, out)
            self.assertEqual(err, "failed: 1 driver without resolvable modes (--strict)\n")
            code, out, _ = self.run_root(root, "--strict", "--json")
            self.assertEqual(code, 1)
            self.assertEqual(json.loads(out)["summary"]["no_modes"], 1)

            # --strict on drivers that all have modes changes nothing
            self.assertEqual(self.run_root(root, "--strict", "--driver", "gc2053")[0::2], (0, ""))

    def test_strict_on_the_fixture_tree(self):
        # no driver without modes there; fx_broken fails the run by itself
        self.assertEqual(run("--strict", "--driver", "gc2053", "--driver", "jxf32")[0], 0)
        self.assertEqual(run("--strict")[2], "failed: 1 driver could not be analysed\n")


@unittest.skipUnless(shutil.which("git"), "git is not installed")
class TreeIdentity(unittest.TestCase):
    """What the JSON output and a baseline say about the checkout."""

    def git(self, *args):
        return subprocess.run(
            ("git", "-C", self.root, "-c", "user.name=test", "-c", "user.email=test@example.invalid",
             "-c", "commit.gpgsign=false", "-c", "core.autocrlf=false") + args,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, check=True).stdout.strip()

    def run_root(self, *args):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
            code = csm.main(["--root", self.root] + list(args))
        return code, out.getvalue()

    def test_baseline_names_the_drivers_commit_not_head(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.root = os.path.join(tmp, "sdk")
            base = os.path.join(self.root, "tools", "baseline.json")
            for rel in (GC2053, "4.4.94/isp/t31/include/tx-isp-common.h"):
                os.makedirs(os.path.dirname(os.path.join(self.root, rel)), exist_ok=True)
                shutil.copyfile(os.path.join(TREE, rel), os.path.join(self.root, rel))
            os.makedirs(os.path.dirname(base))
            self.git("init", "-q")
            self.git("add", "-A")
            self.git("commit", "-q", "-m", "drivers")
            drivers = self.git("rev-parse", "HEAD")

            self.assertEqual(self.run_root("--write-baseline", base)[0], 0)
            with open(base, "r", encoding="utf-8") as fh:
                first = fh.read()
            self.assertEqual(json.loads(first)["tree"], {"drivers_commit": drivers, "dirty": False})

            # the baseline is committed: HEAD moves, the drivers' commit does not,
            # so writing it again changes nothing
            self.git("add", "-A")
            self.git("commit", "-q", "-m", "baseline")
            head = self.git("rev-parse", "HEAD")
            self.assertNotEqual(head, drivers)
            code, out = self.run_root("--json", "--write-baseline", base)
            self.assertEqual(json.loads(out)["tree"],
                             {"commit": head, "drivers_commit": drivers, "dirty": False})
            with open(base, "r", encoding="utf-8") as fh:
                self.assertEqual(fh.read(), first)
            self.assertIn("drivers last changed in " + drivers[:12], self.run_root()[1])

            # an uncommitted change of a driver is said; one outside the drivers is not
            with open(os.path.join(self.root, "tools", "note.txt"), "w") as fh:
                fh.write("x\n")
            self.git("add", "-A")
            self.assertFalse(json.loads(self.run_root("--json")[1])["tree"]["dirty"])
            with open(os.path.join(self.root, GC2053), "a") as fh:
                fh.write("\n")
            self.assertTrue(json.loads(self.run_root("--json")[1])["tree"]["dirty"])

            # a commit that changes a driver moves it
            self.git("commit", "-q", "-a", "-m", "driver change")
            moved = self.git("rev-parse", "HEAD")
            self.assertEqual(json.loads(self.run_root("--json")[1])["tree"]["drivers_commit"], moved)

            # one to another part of the same kernel folder does not, and is not
            # a driver change while uncommitted; one to a header that is read does
            for rel in ("4.4.94/misc/motor/motor.c", "4.4.94/isp/t31/tx-isp-core.c"):
                os.makedirs(os.path.dirname(os.path.join(self.root, rel)), exist_ok=True)
                with open(os.path.join(self.root, rel), "w") as fh:
                    fh.write("/* not a sensor driver */\n")
            self.git("add", "-A")
            self.assertFalse(json.loads(self.run_root("--json")[1])["tree"]["dirty"])
            self.git("commit", "-q", "-m", "motor")
            tree = json.loads(self.run_root("--json")[1])["tree"]
            self.assertEqual((tree["drivers_commit"], tree["dirty"]), (moved, False))
            with open(os.path.join(self.root, "4.4.94/isp/t31/include/tx-isp-common.h"), "a") as fh:
                fh.write("\n")
            self.assertTrue(json.loads(self.run_root("--json")[1])["tree"]["dirty"])
            self.git("commit", "-q", "-a", "-m", "header")
            self.assertEqual(json.loads(self.run_root("--json")[1])["tree"]["drivers_commit"],
                             self.git("rev-parse", "HEAD"))

    def test_not_the_top_of_a_work_tree(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.root = os.path.join(tmp, "sdk")
            for rel in (GC2053, "4.4.94/isp/t31/include/tx-isp-common.h"):
                os.makedirs(os.path.dirname(os.path.join(self.root, rel)), exist_ok=True)
                shutil.copyfile(os.path.join(TREE, rel), os.path.join(self.root, rel))
            code, out = self.run_root("--json")
            self.assertIsNone(json.loads(out)["tree"])
            base = os.path.join(tmp, "baseline.json")
            self.run_root("--write-baseline", base)
            with open(base, "r", encoding="utf-8") as fh:
                self.assertIsNone(json.load(fh)["tree"])


class PreprocessorExpressions(unittest.TestCase):
    DEFINES = {"FLAGS": ("3", 1), "NEXT": ("(FLAGS + 1)", 2), "EMPTY": ("", 3),
               "LOOP_A": ("LOOP_B", 4), "LOOP_B": ("LOOP_A", 5)}

    def cond(self, text):
        return csm.pp_condition("if", text, self.DEFINES)

    def test_true(self):
        for text in ("1", "100/5", "1 & 1", "1<<3", "(FLAGS & 1)", "NEXT == 4", "0x10 == 16",
                     "10UL > 9", "~0 == -1", "!0", "-7/2 == -3", "-7%2 == -1", "1 ? 2 : 0",
                     "defined(FLAGS) && !defined(NOPE)", "defined FLAGS", "FLAGS ^ 1", "0 || FLAGS | 4",
                     "(1 << 2) + 2 * 3 == 10", "FLAGS >= 3 && FLAGS <= 3 && FLAGS != 2"):
            self.assertTrue(self.cond(text), text)

    def test_false(self):
        for text in ("0", "FLAGS & 4", "NOT_A_MACRO", "defined(NOPE)", "1 ? 0 : 2", "FLAGS < 3", "2 - 2"):
            self.assertFalse(self.cond(text), text)

    def test_not_read_is_false_and_reported(self):
        for text in ("9**9**9", "1/0", "1 % 0", "1 << 64", "1 << -1", "VERSION(4,4,0) > 1", "EMPTY",
                     "LOOP_A", "1 +", "(1", "1 2", "__import__('os').system('true')",
                     "08", "0x", "'ab'", "''", "'\\xff'", "'\\400'", "L'a'", "'a"):
            unread = []
            self.assertFalse(csm.pp_condition("if", text, self.DEFINES, unread, 7), text)
            self.assertEqual([(u[0], u[1]) for u in unread], [(7, text)], text)

    def test_operand_c_does_not_evaluate(self):
        # the right side of && and ||, and the arm of ?: that is not taken
        defines = dict(self.DEFINES, DIV0=("(1/0)", 6))
        for text, want in (("0 && (1/0)", 0), ("1 || (1/0)", 1), ("1 ? 2 : (1/0)", 2),
                           ("0 ? (1/0) : 3", 3), ("0 && DIV0", 0), ("1 || 1 << 99", 1),
                           ("1 || 0 && (1/0)", 1), ("(0 && 1/0) + 2", 2), ("0 && 1 || 1", 1),
                           ("0 ? 1 << 64 : 5", 5), ("defined(NOPE) && NOPE / NOPE", 0),
                           ("0 || 2", 1), ("2 && 3", 1), ("0 && (1 ? 1/0 : 1/0)", 0)):
            self.assertEqual(csm.pp_eval(text, defines), want, text)
        # evaluated all the same when the left side does not decide
        for text in ("1 && (1/0)", "0 || (1/0)", "1 ? (1/0) : 2", "0 ? 2 : (1/0)", "1 && DIV0"):
            self.assertRaises(ValueError, csm.pp_eval, text, defines)
        # and still read: what is not an expression is refused there too
        for text in ("0 && (1", "0 && VERSION(4,4,0)", "0 && EMPTY", "0 && 1 2", "1 || 08",
                     "1 ? 2 :", "1 || LOOP_A"):
            self.assertRaises(ValueError, csm.pp_eval, text, defines)

    def test_octal_and_character_constants(self):
        for text, want in (("010", 8), ("0", 0), ("00", 0), ("0777", 511), ("010UL", 8), ("10", 10),
                           ("0x10", 16), ("'a'", 97), ("'A' == 65", 1), ("'0' + 1", 49), ("' '", 32),
                           ("'\\n'", 10), ("'\\t'", 9), ("'\\0'", 0), ("'\\\\'", 92), ("'\\''", 39),
                           ("'\"'", 34), ("'\\\"'", 34), ("'\\x41'", 65), ("'\\101'", 65),
                           ("'\\x7f'", 127), ("'/' / '/'", 1)):
            self.assertEqual(csm.pp_eval(text, {}), want, text)

    def test_ifdef(self):
        self.assertTrue(csm.pp_condition("ifdef", "FLAGS", self.DEFINES))
        self.assertFalse(csm.pp_condition("ifdef", "NOPE", self.DEFINES))
        self.assertTrue(csm.pp_condition("ifndef", "NOPE", self.DEFINES))

    def test_dropped_block_keeps_line_numbers(self):
        text, _ = csm.preprocess("a\n#if (3 & 1)\nb\n#else\nc\n#endif\nd", {})
        self.assertEqual(text.split("\n"), ["a", "", "b", "", "", "", "d"])


class Interpreter(unittest.TestCase):
    """The statement forms the review of the first version found wrong."""

    def run_c(self, body):
        text = "static int f(void)\n{\n%s\n}\n" % body
        unit = csm.Unit("mem.c", "4.4.94/sensor-src/t31/mem.c", text, {})
        it = csm.Interp(unit, {}, None)
        return it.run_func("f", []), it

    def test_goto_forward_to_a_label_of_the_function(self):
        ret, it = self.run_c("int r = 1;\ngoto out;\nr = 2;\nout:\nreturn r;")
        self.assertEqual(ret, 1)
        self.assertEqual(it.gotos, [])

    def test_goto_out_of_nested_blocks(self):
        ret, _ = self.run_c("int r = 0;\nint i;\nfor (i = 0; i < 4; i++) {\n"
                            "if (i == 2) {\nr = 7;\ngoto done;\n}\n}\nr = 9;\ndone:\nreturn r;")
        self.assertEqual(ret, 7)

    def test_goto_backward_ends(self):
        ret, _ = self.run_c("int n = 0;\nagain:\nn = n + 1;\nif (n < 3)\ngoto again;\nreturn n;")
        self.assertEqual(ret, 3)

    def test_goto_backward_without_end_is_stopped(self):
        with self.assertRaises(csm.Abort):
            self.run_c("again:\ngoto again;\nreturn 0;")

    def test_goto_into_a_block_is_reported_not_followed(self):
        ret, it = self.run_c("int r = 1;\ngoto in;\nif (r) {\nin:\nr = 2;\n}\nreturn r;")
        self.assertEqual([(g[0], g[1]) for g in it.gotos], [("f", "in")])

    def test_switch_default_that_does_work_is_taken(self):
        ret, it = self.run_c("int r = 0;\nswitch (5) {\ncase 1:\nr = 1;\nbreak;\ndefault:\nr = 2;\nbreak;\n}\nreturn r;")
        self.assertEqual(ret, 2)
        self.assertEqual(it.nomatch, [])

    def test_switch_default_that_only_logs_is_a_refusal(self):
        ret, it = self.run_c("int r = 0;\nswitch (5) {\ncase 1:\nr = 1;\nbreak;\ndefault:\n"
                             "ISP_ERROR(\"no\\n\");\n}\nreturn r;")
        self.assertEqual(len(it.nomatch), 1)

    def test_switch_without_default(self):
        ret, it = self.run_c("int r = 0;\nswitch (5) {\ncase 1:\nr = 1;\nbreak;\n}\nreturn r;")
        self.assertEqual((ret, len(it.nomatch)), (0, 1))


if __name__ == "__main__":
    unittest.main()

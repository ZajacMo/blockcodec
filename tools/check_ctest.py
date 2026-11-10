#!/usr/bin/env python3
"""Check the result of a ctest run.

ctest only reports whether a test process exited with zero. A test that
skipped its checks, or that reported a failing check of its own, can still
exit with zero and would therefore pass unnoticed. This script runs ctest,
parses the verbose log and fails when

  * a registered suite did not run at all,
  * a suite ran but reported no check,
  * a suite reported a failing check,
  * a suite skipped a check because an input file was missing,
  * the number of suites differs from the expected one.

Usage::

    python tools/check_ctest.py --build-dir build [--config Release]
                                [--expected-suites 10]
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys

# The status word that ctest prints after the dotted line of a test.
KNOWN_STATUSES = (
    "Passed",
    "Failed",
    "Skipped",
    "***Exception",
    "***Timeout",
    "***Not Run",
)

# " 1/10 Test  #1: codec_selftest ...   Passed    0.33 sec"
RESULT_RE = re.compile(
    r"^\s*(\d+)/(\d+)\s+Test\s+#(\d+):\s+(\S+)\s+[.\s]*(\S+)"
)

# "16: 16 checks, 0 failed"
CHECKS_RE = re.compile(r"^\s*(\d+)\s+checks,\s+(\d+)\s+failed\s*$")

# "[       OK ] BitStream.WritesBitsMostSignificantFirst (0 ms)"
OK_RE = re.compile(r"^\s*\[\s*OK\s*\]")

# The summary line that gtest prints at the end of a suite:
# "[  PASSED  ] 4 tests." - it is a summary, not a single check.
PASSED_SUMMARY_RE = re.compile(r"^\s*\[\s*PASSED\s*\]")

# "[  FAILED  ] BitStream.SomeTest (0 ms)"
FAILED_RE = re.compile(r"^\s*\[\s*FAILED\s*\]\s+(.*)$")

# The summary line that gtest prints below the list of failed tests:
# "[  FAILED  ] 2 tests, listed below:"
FAILED_SUMMARY_RE = re.compile(r"^\s*\[\s*FAILED\s*\]\s+\d+\s+tests?,\s+listed below:")

# "[  SKIPPED ] 1 test, listed below:"
SKIPPED_RE = re.compile(r"^\s*\[\s*SKIPPED\s*\]")

# "  [SKIP] images/lena512.bmp (not found)" - the self-test's own marker for
# an input file that could not be found.
SELFTEST_SKIP_RE = re.compile(r"\[SKIP\]")


class Suite:
    """The result of a single ctest suite."""

    def __init__(self, number: int, name: str, status: str) -> None:
        self.number = number
        self.name = name
        self.status = status
        self.checks = 0
        self.failed = 0
        self.skipped = 0

    def __str__(self) -> str:
        return (
            f"  {self.name:<20} {self.status:<10} "
            f"{self.checks:>4} checks, {self.failed} failed"
        )


def run_ctest(build_dir: str, config: str | None) -> tuple[int, str]:
    """Run ctest and return its exit code together with the whole log."""
    ctest = shutil.which("ctest")
    if ctest is None:
        print("ctest was not found on the PATH", file=sys.stderr)
        return 127, ""

    command = [ctest, "--test-dir", build_dir, "-V", "--output-on-failure"]
    if config:
        command[1:1] = ["-C", config]

    print("+ " + " ".join(command), flush=True)
    completed = subprocess.run(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
    )
    sys.stdout.write(completed.stdout)
    sys.stdout.flush()
    return completed.returncode, completed.stdout


def parse(log: str) -> list[Suite]:
    """Extract the suites and their check counts from a verbose ctest log."""
    suites: dict[int, Suite] = {}
    output: dict[int, list[str]] = {}

    for line in log.splitlines():
        # The output of a test is prefixed with its number and a colon.
        prefix = re.match(r"^(\d+): (.*)$", line)
        if prefix:
            output.setdefault(int(prefix.group(1)), []).append(prefix.group(2))
            continue

        result = RESULT_RE.match(line)
        if result:
            total = int(result.group(2))
            number = int(result.group(3))
            name = result.group(4)
            status = result.group(5)
            if status not in KNOWN_STATUSES:
                # The dotted line may run into the status; take the first word
                # that names a status.
                for candidate in KNOWN_STATUSES:
                    if status.startswith(candidate):
                        status = candidate
                        break
                else:
                    status = "Unknown"
            suites[number] = Suite(number, name, status)
            del total

    for number, suite in suites.items():
        lines = output.get(number, [])
        gtest_total = 0
        gtest_failed = 0
        summary_checks = 0
        summary_failed = 0
        for text in lines:
            checks = CHECKS_RE.match(text)
            if checks:
                summary_checks += int(checks.group(1))
                summary_failed += int(checks.group(2))
                continue

            if OK_RE.match(text):
                gtest_total += 1
                continue

            if PASSED_SUMMARY_RE.match(text):
                # The summary line, not a single check.
                continue

            failed = FAILED_RE.match(text)
            if failed:
                if FAILED_SUMMARY_RE.match(text):
                    # The summary line, not the name of a failed test.
                    continue
                gtest_failed += 1
                continue

            if SKIPPED_RE.match(text):
                suite.skipped += 1
                continue

            if SELFTEST_SKIP_RE.search(text):
                suite.skipped += 1
                continue

        # A suite that prints its own summary line has already counted its
        # checks, so the individual lines are not counted a second time.
        if summary_checks:
            suite.checks += summary_checks
            suite.failed += summary_failed
        else:
            suite.checks += gtest_total + gtest_failed
            suite.failed += gtest_failed

    return [suites[number] for number in sorted(suites)]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, help="the directory ctest runs in")
    # The build tree is a multi configuration one, so the configuration has
    # to be named. Without it ctest finds no test at all.
    parser.add_argument("--config", default="Release",
                        help="the build configuration, e.g. Release")
    parser.add_argument(
        "--expected-suites",
        type=int,
        default=10,
        help="the number of suites that have to run",
    )
    args = parser.parse_args()

    code, log = run_ctest(args.build_dir, args.config)
    if not log:
        print("ctest produced no output", file=sys.stderr)
        return 1

    suites = parse(log)
    if not suites:
        print("no test suite was found in the ctest output", file=sys.stderr)
        return 1

    print("\nTest suites")
    for suite in suites:
        print(suite)

    problems = []

    if code != 0:
        problems.append(f"ctest exited with {code}")

    if len(suites) != args.expected_suites:
        problems.append(
            f"{len(suites)} suites ran, expected {args.expected_suites}"
        )

    for suite in suites:
        if suite.status != "Passed":
            problems.append(f"{suite.name} was {suite.status}")
        if suite.checks == 0:
            problems.append(f"{suite.name} ran without a single check")
        if suite.failed:
            problems.append(f"{suite.name} reported {suite.failed} failing checks")
        if suite.skipped:
            problems.append(f"{suite.name} skipped {suite.skipped} checks")

    if problems:
        print("\nThe test run is not acceptable:", file=sys.stderr)
        for problem in problems:
            print(f"  - {problem}", file=sys.stderr)
        return 1

    total = sum(suite.checks for suite in suites)
    print(f"\nAll {len(suites)} suites passed, {total} checks in total.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

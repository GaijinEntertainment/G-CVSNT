#!/usr/bin/env python3
"""Reproduce genbuild's build number from a commit timestamp.

genbuild/genbuild.cpp computes  days_since_epoch(now) - 365*30  and writes
"#define CVSNT_PRODUCT_BUILD <n>" to build.h. genbuild only runs inside the
Windows build, so on Linux build.h is whatever is committed. This script
computes the same number from the committer date of a commit instead of
"now", which makes the value a function of the commit and identical on every
platform that builds it.

Usage:
    build_number.py [--commit REF] [--write PATH]

Prints the number. --write also (re)writes build.h at PATH.
"""

import argparse
import subprocess
import sys

BUILD_FROB = 0          # keep in sync with genbuild.cpp


def build_number(commit_ts):
    return commit_ts // (60 * 60 * 24) - 365 * 30 + BUILD_FROB


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--commit", default="HEAD", help="git ref whose committer date is used")
    ap.add_argument("--write", metavar="PATH", help="write build.h to PATH")
    args = ap.parse_args()

    ts = int(subprocess.check_output(
        ["git", "log", "-1", "--format=%ct", args.commit], universal_newlines=True).strip())
    n = build_number(ts)
    if args.write:
        with open(args.write, "w", newline="\n") as f:
            f.write("#define CVSNT_PRODUCT_BUILD %d\n" % n)
    print(n)
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Local-repository regression tests for G-CVSNT.

Exercises the code paths that carry the highest risk of silent regression:
Entries bookkeeping, sticky tags, the -C / -n backup rules, tag and branch
numbering, and directory pruning.  Everything runs against a local repository,
so no server, no lock server and no blob store are required.

Usage:
    python regress.py --cvs <path-to-cvs> [--libdir <dir>] [-v]

--libdir is the directory holding the protocol/trigger plugins; it becomes the
global -L option.  It is needed when cvs is run from a build tree rather than
an installation.

Exit status is 0 if every test passed.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

CVS = None
LIBDIR = None
VERBOSE = False

FAILURES = []
PASSED = 0
CURRENT = "<none>"


# --------------------------------------------------------------------------- infra

def run(args, cwd, expect_ok=True):
    """Run cvs with the global options, return (rc, stdout+stderr)."""
    cmd = [CVS]
    if LIBDIR:
        cmd += ["-L", LIBDIR]
    cmd += args
    if VERBOSE:
        print("    $ " + " ".join(cmd) + ("   (in %s)" % cwd))
    p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, universal_newlines=True)
    if VERBOSE and p.stdout:
        print("      " + p.stdout.rstrip().replace("\n", "\n      "))
    if expect_ok and p.returncode != 0:
        fail("command failed (rc=%d): %s\n%s" % (p.returncode, " ".join(args), p.stdout))
    return p.returncode, p.stdout


def fail(msg):
    FAILURES.append((CURRENT, msg))


def check(cond, msg):
    if not cond:
        fail(msg)
    return cond


def check_eq(got, want, what):
    if got != want:
        fail("%s: expected %r, got %r" % (what, want, got))
        return False
    return True


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="\n") as f:
        f.write(text)


def read(path):
    with open(path, "r") as f:
        return f.read()


def entries_of(wcdir):
    """Parse CVS/Entries (plus Entries.Log) into {name: line}."""
    out = {}
    for fn in ("Entries", "Entries.Log"):
        p = os.path.join(wcdir, "CVS", fn)
        if not os.path.exists(p):
            continue
        for line in read(p).splitlines():
            body = line
            cmd = None
            if fn == "Entries.Log":
                if len(line) > 1 and line[1] == " ":
                    cmd, body = line[0], line[2:]
                else:
                    cmd = "A"
            if not body.startswith("/") and not body.startswith("D/"):
                continue
            name = body.lstrip("D").split("/")[1] if body.startswith("D/") else body.split("/")[1]
            if cmd == "R":
                out.pop(name, None)
            else:
                out[name] = body
    return out


class Repo:
    """A throwaway repository plus a working copy."""

    def __init__(self, root):
        self.root = root
        self.repo = os.path.join(root, "repo")
        self.wc = os.path.join(root, "wc")
        os.makedirs(self.repo)
        os.makedirs(self.wc)
        # -n: skip registering the repository in the machine-global settings,
        # which needs privileges we do not want a test to require.
        run(["-d", self.repo, "init", "-n"], cwd=root)

    def cvs(self, args, cwd=None, expect_ok=True):
        return run(["-d", self.repo] + args, cwd=cwd or self.wc, expect_ok=expect_ok)

    def import_tree(self, module, files):
        imp = os.path.join(self.root, "imp")
        if os.path.exists(imp):
            shutil.rmtree(imp)
        os.makedirs(imp)
        for rel, text in files.items():
            write(os.path.join(imp, rel), text)
        self.cvs(["import", "-m", "initial", module, "VENDOR", "REL0"], cwd=imp)

    def checkout(self, module):
        self.cvs(["checkout", module])
        return os.path.join(self.wc, module)


def test(name):
    def deco(fn):
        fn._test_name = name
        return fn
    return deco


# --------------------------------------------------------------------------- tests

@test("import, checkout and log round trip")
def t_roundtrip(r):
    r.import_tree("m", {"a.txt": "one\n", "sub/b.txt": "two\n"})
    wc = r.checkout("m")
    check(os.path.isfile(os.path.join(wc, "a.txt")), "a.txt not checked out")
    check(os.path.isfile(os.path.join(wc, "sub", "b.txt")), "sub/b.txt not checked out")
    check_eq(read(os.path.join(wc, "a.txt")), "one\n", "a.txt content")
    _, out = r.cvs(["log", "a.txt"], cwd=wc)
    check("Initial revision" in out, "log lacks the initial revision")


@test("commit creates a new revision")
def t_commit(r):
    r.import_tree("m", {"a.txt": "one\n"})
    wc = r.checkout("m")
    write(os.path.join(wc, "a.txt"), "one\ntwo\n")
    _, out = r.cvs(["commit", "-m", "second"], cwd=wc)
    check("new revision: 1.2" in out, "commit did not produce revision 1.2:\n" + out)
    _, out = r.cvs(["status", "a.txt"], cwd=wc)
    check(re.search(r"Working revision:\s+1\.2", out) is not None,
          "status does not report 1.2:\n" + out)


@test("Entries has exactly one line per file after commit")
def t_entries_no_duplicates(r):
    # Guards against a regression where each Entries.Log record was written
    # twice, the second copy without its command prefix.
    r.import_tree("m", {"a.txt": "one\n", "c.txt": "three\n"})
    wc = r.checkout("m")
    write(os.path.join(wc, "a.txt"), "one\nchanged\n")
    r.cvs(["commit", "-m", "x"], cwd=wc)
    for fn in ("Entries", "Entries.Log"):
        p = os.path.join(wc, "CVS", fn)
        if not os.path.exists(p):
            continue
        lines = [l for l in read(p).splitlines() if l.strip() and l.strip() != "D"]
        seen = {}
        for l in lines:
            seen[l] = seen.get(l, 0) + 1
        dupes = {k: v for k, v in seen.items() if v > 1}
        check(not dupes, "%s has duplicate lines: %r" % (fn, dupes))


@test("tag and branch produce the right revision numbers")
def t_tag_branch(r):
    r.import_tree("m", {"a.txt": "one\n"})
    wc = r.checkout("m")
    write(os.path.join(wc, "a.txt"), "one\ntwo\n")
    r.cvs(["commit", "-m", "second"], cwd=wc)
    r.cvs(["tag", "REL1"], cwd=wc)
    r.cvs(["tag", "-b", "BR1"], cwd=wc)
    _, out = r.cvs(["log", "a.txt"], cwd=wc)
    m = re.search(r"^\tREL1:\s*(\S+)$", out, re.M)
    check(m is not None, "REL1 missing from symbolic names:\n" + out)
    if m:
        check_eq(m.group(1), "1.2", "REL1 revision")
    m = re.search(r"^\tBR1:\s*(\S+)$", out, re.M)
    check(m is not None, "BR1 missing from symbolic names:\n" + out)
    if m:
        # A branch tag is stored as a magic revision: <branchpoint>.0.<n>
        check(re.match(r"^1\.2\.0\.\d+$", m.group(1)) is not None,
              "BR1 is not a magic branch revision: %r" % m.group(1))


@test("update -r makes the tag sticky, update -A clears it")
def t_sticky(r):
    r.import_tree("m", {"a.txt": "one\n"})
    wc = r.checkout("m")
    r.cvs(["tag", "REL1"], cwd=wc)
    write(os.path.join(wc, "a.txt"), "one\ntwo\n")
    r.cvs(["commit", "-m", "second"], cwd=wc)

    r.cvs(["update", "-r", "REL1"], cwd=wc)
    check_eq(read(os.path.join(wc, "a.txt")), "one\n", "content at REL1")
    _, out = r.cvs(["status", "a.txt"], cwd=wc)
    check(re.search(r"Sticky Tag:\s+REL1", out) is not None,
          "sticky tag not set after update -r:\n" + out)

    r.cvs(["update", "-A"], cwd=wc)
    check_eq(read(os.path.join(wc, "a.txt")), "one\ntwo\n", "content after update -A")
    _, out = r.cvs(["status", "a.txt"], cwd=wc)
    check(re.search(r"Sticky Tag:\s+\(none\)", out) is not None,
          "sticky tag not cleared by update -A:\n" + out)


@test("update -C reverts and leaves a .# backup")
def t_update_C_backup(r):
    r.import_tree("m", {"a.txt": "one\n"})
    wc = r.checkout("m")
    write(os.path.join(wc, "a.txt"), "locally modified\n")
    r.cvs(["update", "-C"], cwd=wc)
    check_eq(read(os.path.join(wc, "a.txt")), "one\n", "content after update -C")
    backups = [f for f in os.listdir(wc) if f.startswith(".#a.txt")]
    check(backups, "update -C left no .#a.txt backup")
    if backups:
        check_eq(read(os.path.join(wc, backups[0])), "locally modified\n",
                 "backup content")


@test("update -C -n reverts without leaving a backup")
def t_update_C_nobackup(r):
    r.import_tree("m", {"a.txt": "one\n"})
    wc = r.checkout("m")
    write(os.path.join(wc, "a.txt"), "locally modified\n")
    r.cvs(["update", "-C", "-n"], cwd=wc)
    check_eq(read(os.path.join(wc, "a.txt")), "one\n", "content after update -C -n")
    backups = [f for f in os.listdir(wc) if f.startswith(".#a.txt")]
    check(not backups, "update -C -n left a backup: %r" % backups)


@test("update -d picks up a directory added after checkout")
def t_update_d(r):
    r.import_tree("m", {"a.txt": "one\n"})
    wc = r.checkout("m")
    # Add a second module directory through a separate import, then update.
    imp = os.path.join(r.root, "imp2")
    os.makedirs(os.path.join(imp, "newdir"))
    write(os.path.join(imp, "newdir", "n.txt"), "new\n")
    r.cvs(["import", "-m", "add dir", "m/newdir", "VENDOR", "REL0"],
          cwd=os.path.join(imp, "newdir"))

    r.cvs(["update"], cwd=wc)
    check(not os.path.isdir(os.path.join(wc, "newdir")),
          "update without -d created newdir anyway")

    r.cvs(["update", "-d"], cwd=wc)
    check(os.path.isfile(os.path.join(wc, "newdir", "n.txt")),
          "update -d did not bring in newdir/n.txt")


@test("remove plus commit plus update -P prunes the empty directory")
def t_prune(r):
    r.import_tree("m", {"a.txt": "one\n", "sub/b.txt": "two\n"})
    wc = r.checkout("m")
    sub = os.path.join(wc, "sub")
    os.remove(os.path.join(sub, "b.txt"))
    r.cvs(["remove", "b.txt"], cwd=sub)
    r.cvs(["commit", "-m", "drop b"], cwd=sub)
    r.cvs(["update", "-P"], cwd=wc)
    check(not os.path.isdir(sub), "update -P did not prune the empty sub/")


@test("binary file survives a commit/checkout round trip byte for byte")
def t_binary(r):
    payload = bytes(range(256)) * 8
    imp = os.path.join(r.root, "impbin")
    os.makedirs(imp)
    with open(os.path.join(imp, "bin.dat"), "wb") as f:
        f.write(payload)
    r.cvs(["import", "-m", "bin", "-kb", "mb", "VENDOR", "REL0"], cwd=imp)
    r.cvs(["checkout", "mb"])
    got = open(os.path.join(r.wc, "mb", "bin.dat"), "rb").read()
    check_eq(got, payload, "binary round trip")


@test("a second checkout of the same module matches the first")
def t_second_checkout(r):
    r.import_tree("m", {"a.txt": "one\n", "sub/b.txt": "two\n"})
    wc1 = r.checkout("m")
    write(os.path.join(wc1, "a.txt"), "one\nedited\n")
    r.cvs(["commit", "-m", "edit"], cwd=wc1)

    wc2root = os.path.join(r.root, "wc2")
    os.makedirs(wc2root)
    r.cvs(["checkout", "m"], cwd=wc2root)
    for rel in ("a.txt", os.path.join("sub", "b.txt")):
        a = read(os.path.join(wc1, rel))
        b = read(os.path.join(wc2root, "m", rel))
        check_eq(b, a, "second checkout differs for " + rel)


# --------------------------------------------------------------------------- driver

def main():
    global CVS, LIBDIR, VERBOSE, CURRENT, PASSED

    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cvs", required=True, help="path to the cvs executable under test")
    ap.add_argument("--libdir", help="plugin directory, passed as the global -L option")
    ap.add_argument("--keep", action="store_true", help="do not delete the scratch directory")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    CVS = os.path.abspath(args.cvs)
    LIBDIR = os.path.abspath(args.libdir) if args.libdir else None
    VERBOSE = args.verbose

    if not os.path.exists(CVS):
        print("no such executable: " + CVS)
        return 2

    tests = [v for v in globals().values()
             if callable(v) and hasattr(v, "_test_name")]
    tests.sort(key=lambda f: f.__code__.co_firstlineno)

    scratch = tempfile.mkdtemp(prefix="cvsregress-")
    print("scratch: " + scratch)
    print()

    for fn in tests:
        CURRENT = fn._test_name
        before = len(FAILURES)
        root = os.path.join(scratch, "t%02d" % (tests.index(fn) + 1))
        os.makedirs(root)
        try:
            fn(Repo(root))
        except Exception as e:  # noqa: BLE001 - a crashing test is a failed test
            fail("raised %s: %s" % (type(e).__name__, e))
        if len(FAILURES) == before:
            PASSED += 1
            print("  ok    " + CURRENT)
        else:
            print("  FAIL  " + CURRENT)
            for _, msg in FAILURES[before:]:
                print("          " + msg.replace("\n", "\n          "))

    print()
    print("%d passed, %d failed" % (PASSED, len(tests) - PASSED))

    if not args.keep:
        shutil.rmtree(scratch, ignore_errors=True)
    else:
        print("kept: " + scratch)

    return 0 if not FAILURES else 1


if __name__ == "__main__":
    sys.exit(main())

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
XFAILED = []
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

def xfail(name, reason):
    """A test that pins a known-open defect: it is expected to fail today.
    Reported XFAIL when it fails (no suite failure) and XPASS when it
    unexpectedly passes - which means the defect is fixed and the marker
    should come off.  An XPASS counts as a suite failure so it is not
    missed."""
    def deco(fn):
        fn._test_name = name
        fn._xfail = reason
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


def _merge_setup(r):
    """Set up a repository where both merge producers can fire: rev 1.2 on
    the trunk, a branch JBR with one commit on it (for -j joins), and the
    first working copy left at its original revision with a non-conflicting
    local edit, so that an update there has to merge.  Returns that copy."""
    r.import_tree("m", {"a.txt": "line one\nline two\nline three\n"})
    wc = r.checkout("m")
    wc2root = os.path.join(r.root, "wc2")
    os.makedirs(wc2root)
    r.cvs(["checkout", "m"], cwd=wc2root)
    wc2 = os.path.join(wc2root, "m")
    write(os.path.join(wc2, "a.txt"), "line one\nline two\nthree changed\n")
    r.cvs(["commit", "-m", "second"], cwd=wc2)           # -> 1.2
    r.cvs(["tag", "-b", "JBR"], cwd=wc2)
    r.cvs(["update", "-r", "JBR"], cwd=wc2)
    write(os.path.join(wc2, "a.txt"), "one branch\nline two\nthree changed\n")
    r.cvs(["commit", "-m", "on branch"], cwd=wc2)        # -> 1.2.2.1
    write(os.path.join(wc, "a.txt"), "one local\nline two\nline three\n")
    return wc


MERGED = "one local\nline two\nthree changed\n"
JOINED = "one branch\nline two\nthree changed\n"


def _join_wc(r, sub):
    """A fresh trunk working copy for a -j join."""
    root = os.path.join(r.root, sub)
    os.makedirs(root)
    r.cvs(["checkout", "m"], cwd=root)
    return os.path.join(root, "m")


@test("merging updates leave .# backups of the pre-merge files")
def t_merge_backup_default(r):
    wc = _merge_setup(r)
    r.cvs(["update"], cwd=wc)
    check_eq(read(os.path.join(wc, "a.txt")), MERGED, "merged content")
    backups = [f for f in os.listdir(wc) if f.startswith(".#a.txt")]
    check(backups, "merge left no .#a.txt backup")
    if backups:
        check_eq(read(os.path.join(wc, backups[0])),
                 "one local\nline two\nline three\n", "backup content")

    # A -j join backs up the pre-join file the same way.
    wc3 = _join_wc(r, "wc3")
    r.cvs(["update", "-j", "JBR", "a.txt"], cwd=wc3)
    check_eq(read(os.path.join(wc3, "a.txt")), JOINED, "joined content")
    backups = [f for f in os.listdir(wc3) if f.startswith(".#a.txt")]
    check(backups, "join left no .#a.txt backup")


@test("update -n / --no-backups merges without leaving .# backups")
def t_merge_no_backup(r):
    wc = _merge_setup(r)
    r.cvs(["update", "-n"], cwd=wc)
    check_eq(read(os.path.join(wc, "a.txt")), MERGED,
             "merged content with -n")
    backups = [f for f in os.listdir(wc) if f.startswith(".#")]
    check(not backups, "update -n left backups: %r" % backups)

    wc3 = _join_wc(r, "wc3")
    r.cvs(["update", "--no-backups", "-j", "JBR", "a.txt"], cwd=wc3)
    check_eq(read(os.path.join(wc3, "a.txt")), JOINED,
             "joined content with --no-backups")
    backups = [f for f in os.listdir(wc3) if f.startswith(".#")]
    check(not backups, "update --no-backups left backups: %r" % backups)


@test("update -n on a nonmergeable conflict installs the repository revision and keeps no copy")
def t_no_backup_nonmergeable(r):
    # The most destructive -n consequence, pinned: a -kb file with a local
    # edit meets a newer repository revision.  A binary merge is a conflict
    # by definition, so the repository revision replaces the local file,
    # and the pre-merge copy that would have held the local edit is
    # removed before the command returns - nothing names where it went.
    payload1 = bytes(range(256)) * 2
    payload2 = bytes(reversed(range(256))) * 3
    local = b"local edit " * 40
    imp = os.path.join(r.root, "impbin")
    os.makedirs(imp)
    with open(os.path.join(imp, "b.dat"), "wb") as f:
        f.write(payload1)
    r.cvs(["import", "-m", "bin", "-kb", "mb", "VENDOR", "REL0"], cwd=imp)
    wc = r.checkout("mb")
    wc2root = os.path.join(r.root, "wc2")
    os.makedirs(wc2root)
    r.cvs(["checkout", "mb"], cwd=wc2root)
    wc2 = os.path.join(wc2root, "mb")
    with open(os.path.join(wc2, "b.dat"), "wb") as f:
        f.write(payload2)
    r.cvs(["commit", "-m", "second"], cwd=wc2)
    with open(os.path.join(wc, "b.dat"), "wb") as f:
        f.write(local)

    rc, out = r.cvs(["update", "-n"], cwd=wc, expect_ok=False)
    check("nonmergeable file needs merge" in out,
          "update -n did not report the nonmergeable conflict:" + chr(10) + out)
    check("file from working directory is now in" not in out,
          "update -n named a copy it does not keep:" + chr(10) + out)
    check_eq(open(os.path.join(wc, "b.dat"), "rb").read(), payload2,
             "b.dat content after update -n on a nonmergeable conflict")
    backups = [f for f in os.listdir(wc) if f.startswith(".#")]
    check(not backups, "update -n left a pre-merge copy: %r" % backups)


def _in_the_way_setup(r):
    """Commit a new file b.txt from a second working copy and obstruct its
    path in the first working copy with an unversioned file.  Returns the
    first working copy."""
    r.import_tree("m", {"a.txt": "aaa\n"})
    wc = r.checkout("m")
    wc2root = os.path.join(r.root, "wc2")
    os.makedirs(wc2root)
    r.cvs(["checkout", "m"], cwd=wc2root)
    wc2 = os.path.join(wc2root, "m")
    write(os.path.join(wc2, "b.txt"), "repo version\n")
    r.cvs(["add", "b.txt"], cwd=wc2)
    r.cvs(["commit", "-m", "add b"], cwd=wc2)
    write(os.path.join(wc, "b.txt"), "local stuff\n")
    return wc


@test("an unversioned file in the way blocks the update, run after run")
def t_in_the_way_default(r):
    wc = _in_the_way_setup(r)
    for attempt in ("first", "second"):
        rc, out = r.cvs(["update"], cwd=wc, expect_ok=False)
        check(rc != 0, "%s update with an obstruction exited 0" % attempt)
        check("move away b.txt; it is in the way" in out,
              "%s update lacks the move-away message:\n%s" % (attempt, out))
        check_eq(read(os.path.join(wc, "b.txt")), "local stuff\n",
                 "obstructing file content after %s update" % attempt)
        check(not [f for f in os.listdir(wc) if f.startswith(".#")],
              "update without the option created .# files")


@test("update --move-in-the-way renames the obstruction and converges")
def t_in_the_way_moved(r):
    wc = _in_the_way_setup(r)
    rc, out = r.cvs(["update", "--move-in-the-way"], cwd=wc, expect_ok=False)
    check_eq(rc, 0, "update --move-in-the-way exit status:\n" + out)
    check("move away" not in out, "still asks to move away:\n" + out)
    check_eq(read(os.path.join(wc, "b.txt")), "repo version\n",
             "b.txt content after recovery")
    aside = [f for f in os.listdir(wc) if f.startswith(".#b.txt.notversioned.")]
    check_eq(len(aside), 1, "aside backups: %r" % aside)
    if aside:
        check_eq(read(os.path.join(wc, aside[0])), "local stuff\n",
                 "aside backup content")
    rc, out = r.cvs(["update"], cwd=wc, expect_ok=False)
    check_eq(rc, 0, "plain update after recovery is not clean:\n" + out)

    # checkout into a pre-populated directory shares the option.
    wc3root = os.path.join(r.root, "wc3")
    os.makedirs(os.path.join(wc3root, "m"))
    write(os.path.join(wc3root, "m", "b.txt"), "other local\n")
    rc, out = r.cvs(["checkout", "--move-in-the-way", "m"], cwd=wc3root,
                    expect_ok=False)
    check_eq(rc, 0, "checkout --move-in-the-way exit status:\n" + out)
    check_eq(read(os.path.join(wc3root, "m", "b.txt")), "repo version\n",
             "b.txt content after checkout recovery")
    aside = [f for f in os.listdir(os.path.join(wc3root, "m"))
             if f.startswith(".#b.txt.notversioned.")]
    check_eq(len(aside), 1, "checkout aside backups: %r" % aside)


@test("a missing CVS/Entries aborts the whole update")
def t_missing_entries_default(r):
    r.import_tree("m", {"a.txt": "aaa\n", "sub/b.txt": "bbb\n",
                        "sub/nested/d.txt": "ddd\n"})
    wc = r.checkout("m")
    os.remove(os.path.join(wc, "sub", "CVS", "Entries"))
    rc, out = r.cvs(["update"], cwd=wc, expect_ok=False)
    check(rc != 0, "update with a missing Entries exited 0")
    check("CVS/Entries is missing" in out,
          "missing-Entries message absent:\n" + out)
    check(not os.path.exists(os.path.join(wc, "sub", "CVS", "Entries")),
          "update recreated Entries without being asked to")


@test("update --recreate-entries repairs a missing CVS/Entries")
def t_missing_entries_recreated(r):
    r.import_tree("m", {"a.txt": "aaa\n", "sub/b.txt": "bbb\n",
                        "sub/nested/d.txt": "ddd\n"})
    wc = r.checkout("m")
    os.remove(os.path.join(wc, "sub", "CVS", "Entries"))
    rc, out = r.cvs(["update", "--recreate-entries"], cwd=wc, expect_ok=False)
    check_eq(rc, 0, "update --recreate-entries exit status:\n" + out)
    check("recreated missing" in out, "no recreation notice:\n" + out)
    ents = entries_of(os.path.join(wc, "sub"))
    check("b.txt" in ents, "b.txt not re-registered: %r" % ents)
    check("nested" in ents, "nested/ lost from the recreated Entries: %r" % ents)
    check_eq(read(os.path.join(wc, "sub", "b.txt")), "bbb\n", "b.txt content")
    check_eq(read(os.path.join(wc, "sub", "nested", "d.txt")), "ddd\n",
             "nested/d.txt content")
    rc, out = r.cvs(["update"], cwd=wc, expect_ok=False)
    check_eq(rc, 0, "plain update after the repair is not clean:\n" + out)


@test("--rename-in-use is accepted globally and inert when nothing is locked")
def t_rename_in_use_inert(r):
    # The recovery branch itself (destination in use when the temp file is
    # renamed over it) is reachable only through the client/server
    # update_entries path with a running image holding the file: a local
    # update -C renames the old file aside first, and a mapping held by a
    # plain file handle blocks that rename too, so it cannot be driven from
    # this local-mode suite.  A real test needs the piped-server harness
    # and a spawned executable; recorded as open.  Here we pin that the
    # global switch parses and is a no-op on the ordinary path.
    r.import_tree("m", {"a.txt": "one\n"})
    wc = r.checkout("m")
    write(os.path.join(wc, "a.txt"), "one\ntwo\n")
    r.cvs(["commit", "-m", "second"], cwd=wc)
    write(os.path.join(wc, "a.txt"), "local\n")
    rc, out = run(["--rename-in-use", "-d", r.repo, "update", "-C"], cwd=wc,
                  expect_ok=False)
    check_eq(rc, 0, "update -C with --rename-in-use exit status:\n" + out)
    check_eq(read(os.path.join(wc, "a.txt")), "one\ntwo\n",
             "content after update -C with the switch")
    check(not [f for f in os.listdir(wc) if ".inuse." in f],
          "the switch created an inuse aside with nothing locked")


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


@test("second commit of a binary file round trips byte for byte")
def t_binary_second_commit(r):
    # Import only exercises the inline-storage path; a *commit* goes through
    # RCS_checkin, which routes binary content into the content-addressed
    # blob store.  In local mode that store's root must resolve to the
    # repository - not the process working directory - and must work in a
    # repository that has no blobs/ directory yet, or the second revision of
    # every binary file either aborts or is silently lost.
    payload1 = bytes(range(256)) * 4
    payload2 = bytes(reversed(range(256))) * 6 + b"\x00tail"
    imp = os.path.join(r.root, "impbin")
    os.makedirs(imp)
    with open(os.path.join(imp, "b.dat"), "wb") as f:
        f.write(payload1)
    r.cvs(["import", "-m", "bin", "-kb", "mb", "VENDOR", "REL0"], cwd=imp)
    wc = r.checkout("mb")

    with open(os.path.join(wc, "b.dat"), "wb") as f:
        f.write(payload2)
    _, out = r.cvs(["commit", "-m", "second"], cwd=wc)
    check("new revision" in out, "second binary commit did not succeed:\n" + out)

    # The blob must not have been sprayed into the working copy.
    check(not os.path.isdir(os.path.join(wc, "blobs")),
          "commit created a blobs/ directory inside the working copy")

    # A fresh checkout elsewhere must reproduce the committed bytes exactly.
    wc2root = os.path.join(r.root, "wc2")
    os.makedirs(wc2root)
    r.cvs(["checkout", "mb"], cwd=wc2root)
    got = open(os.path.join(wc2root, "mb", "b.dat"), "rb").read()
    check_eq(len(got), len(payload2), "second-revision size after fresh checkout")
    check_eq(got, payload2, "second-revision content after fresh checkout")

    # And the first revision must still be reachable.
    r.cvs(["update", "-r", "1.1.1.1", "b.dat"], cwd=os.path.join(wc2root, "mb"))
    got = open(os.path.join(wc2root, "mb", "b.dat"), "rb").read()
    check_eq(got, payload1, "first-revision content via update -r")


@test("interrupted checkout leaves well-formed Entries logs")
def t_entries_log_format(r):
    # A checkout that dies mid-directory (here: on a corrupt ,v) leaves
    # CVS/Entries.Log and CVS/Entries.Extra.Log behind for the next command
    # to replay.  Every record in them must be complete: a command prefix
    # ("A " / "R ") is optional, but a prefix with no record after it is
    # corruption.  Replaying the surviving log must yield exactly the files
    # that were checked out before the failure.
    r.import_tree("m", {"a.txt": "one\n", "b.txt": "two\n", "z.txt": "three\n"})
    zv = os.path.join(r.repo, "m", "z.txt,v")
    os.chmod(zv, 0o644)                 # ,v files are checked in read-only
    with open(zv, "w") as f:
        f.write("this is not an rcs file\n")
    wcroot = os.path.join(r.root, "wcbad")
    os.makedirs(wcroot)
    rc, out = r.cvs(["checkout", "m"], cwd=wcroot, expect_ok=False)
    check(rc != 0, "checkout of a corrupt ,v unexpectedly succeeded")
    wc = os.path.join(wcroot, "m")
    check(os.path.isfile(os.path.join(wc, "a.txt")), "a.txt missing after partial checkout")
    check(os.path.isfile(os.path.join(wc, "b.txt")), "b.txt missing after partial checkout")
    for fn in ("Entries.Log", "Entries.Extra.Log"):
        p = os.path.join(wc, "CVS", fn)
        if not os.path.exists(p):
            continue
        for line in read(p).splitlines():
            if not line.strip():
                continue
            body = line
            if len(line) >= 2 and line[1] == " " and line[0] in "AR":
                body = line[2:]
            check(body.startswith("/") or body.startswith("D/"),
                  "%s has a malformed record: %r" % (fn, line))
    ents = entries_of(wc)
    check_eq(sorted(ents), ["a.txt", "b.txt"], "entries after interrupted checkout")


@test("rtag, tag -F and tag -d update the symbol table correctly")
def t_tag_move_delete(r):
    r.import_tree("m", {"a.txt": "one\n", "sub/b.txt": "two\n"})
    wc = r.checkout("m")
    r.cvs(["tag", "T1"], cwd=wc)                       # T1 -> 1.1
    write(os.path.join(wc, "a.txt"), "one\ntwo\n")
    r.cvs(["commit", "-m", "second"], cwd=wc)          # a.txt -> 1.2
    r.cvs(["tag", "-F", "T1"], cwd=wc)                 # move T1 -> 1.2
    r.cvs(["rtag", "-r", "T1", "R1", "m"])             # R1 -> wherever T1 is
    r.cvs(["rtag", "-b", "BR2", "m"])                  # branch tag off head

    _, out = r.cvs(["log", "a.txt"], cwd=wc)
    for tag, pat in (("T1", r"^1\.2$"), ("R1", r"^1\.2$"), ("BR2", r"^1\.2\.0\.\d+$")):
        m = re.search(r"^\t%s:\s*(\S+)$" % tag, out, re.M)
        check(m is not None, "%s missing from symbolic names:\n%s" % (tag, out))
        if m:
            check(re.match(pat, m.group(1)) is not None,
                  "%s points at %r, expected %r" % (tag, m.group(1), pat))

    r.cvs(["tag", "-d", "T1"], cwd=wc)                 # delete via tag
    r.cvs(["rtag", "-d", "R1", "m"])                   # delete via rtag
    _, out = r.cvs(["log", "a.txt"], cwd=wc)
    check(re.search(r"^\tT1:", out, re.M) is None, "T1 still present after tag -d:\n" + out)
    check(re.search(r"^\tR1:", out, re.M) is None, "R1 still present after rtag -d:\n" + out)
    check(re.search(r"^\tBR2:", out, re.M) is not None, "BR2 lost by the deletes:\n" + out)

    # The rewritten ,v files must still be fully usable afterwards.
    write(os.path.join(wc, "a.txt"), "one\ntwo\nthree\n")
    _, out = r.cvs(["commit", "-m", "third"], cwd=wc)
    check("new revision: 1.3" in out, "commit after tagging did not produce 1.3:\n" + out)
    r.cvs(["update", "-r", "BR2"], cwd=wc)
    check_eq(read(os.path.join(wc, "a.txt")), "one\ntwo\n", "content on branch BR2")
    r.cvs(["update", "-A"], cwd=wc)
    check_eq(read(os.path.join(wc, "a.txt")), "one\ntwo\nthree\n", "content back on head")


@test("text file bigger than the RCS parse buffer round trips")
def t_large_text(r):
    # ~205 KiB, which crosses several 64 KiB RCSBUF_BUFSIZE boundaries in
    # rcsbuf_fill and exercises the buffered delta walk; the @s exercise RCS
    # @-escaping.  See t_huge_text for the regime past MAX_INCR.
    payload = "".join("line %05d %s @@ at@sign\n" % (i, "x" * (i % 60))
                      for i in range(4000))
    r.import_tree("m", {"big.txt": payload})
    wc = r.checkout("m")
    check_eq(read(os.path.join(wc, "big.txt")), payload, "content after first checkout")

    write(os.path.join(wc, "big.txt"), payload + "tail\n")
    r.cvs(["commit", "-m", "second"], cwd=wc)

    r.cvs(["update", "-r", "1.1", "big.txt"], cwd=wc)
    check_eq(read(os.path.join(wc, "big.txt")), payload, "content of revision 1.1")
    r.cvs(["update", "-A", "big.txt"], cwd=wc)
    check_eq(read(os.path.join(wc, "big.txt")), payload + "tail\n", "content of head")

    wc2root = os.path.join(r.root, "wc2")
    os.makedirs(wc2root)
    r.cvs(["checkout", "m"], cwd=wc2root)
    check_eq(read(os.path.join(wc2root, "m", "big.txt")), payload + "tail\n",
             "content after second checkout")


@test("empty file round trips")
def t_empty_file(r):
    # Exercises the st_size == 0 branch of the RCS parse-buffer pre-size,
    # which is skipped and must fall back to incremental growth.
    r.import_tree("m", {"empty.txt": "", "a.txt": "one\n"})
    wc = r.checkout("m")
    check(os.path.isfile(os.path.join(wc, "empty.txt")), "empty.txt not checked out")
    check_eq(read(os.path.join(wc, "empty.txt")), "", "empty file content")
    write(os.path.join(wc, "empty.txt"), "now has content\n")
    r.cvs(["commit", "-m", "fill it"], cwd=wc)
    r.cvs(["update", "-r", "1.1", "empty.txt"], cwd=wc)
    check_eq(read(os.path.join(wc, "empty.txt")), "", "empty file at revision 1.1")


@test("text file past the RCS buffer growth threshold round trips")
def t_huge_text(r):
    # The parse-buffer pre-size exists for ,v files past MAX_INCR (2 MiB),
    # where expand_string stops doubling and grows by a constant instead.  The
    # 205 KiB case above never leaves the geometric regime, so it cannot show a
    # regression in the path the optimization actually targets.  ~3 MiB puts
    # the ,v above MAX_INCR and below the 8 MiB pre-size cap.
    line = "the quick brown fox jumps over the lazy dog 0123456789 @@\n"
    payload = line * (3 * 1024 * 1024 // len(line))
    r.import_tree("m", {"huge.txt": payload})
    wc = r.checkout("m")
    check_eq(len(read(os.path.join(wc, "huge.txt"))), len(payload), "huge file size")
    check_eq(read(os.path.join(wc, "huge.txt")), payload, "huge file content")

    write(os.path.join(wc, "huge.txt"), payload + "tail\n")
    r.cvs(["commit", "-m", "second"], cwd=wc)
    r.cvs(["update", "-r", "1.1", "huge.txt"], cwd=wc)
    check_eq(read(os.path.join(wc, "huge.txt")), payload, "huge file at revision 1.1")
    r.cvs(["update", "-A", "huge.txt"], cwd=wc)
    check_eq(read(os.path.join(wc, "huge.txt")), payload + "tail\n", "huge file at head")

    # Tagging rewrites the whole ,v, which is the RCS_copydeltas path.
    r.cvs(["tag", "REL1"], cwd=wc)
    r.cvs(["update", "-r", "REL1", "huge.txt"], cwd=wc)
    check_eq(read(os.path.join(wc, "huge.txt")), payload + "tail\n",
             "huge file content at REL1 after the ,v was rewritten")


@test("a ,v present in both the repository and the Attic is listed once")
def t_attic_duplicate(r):
    r.import_tree("m", {"a.txt": "one\n", "b.txt": "two\n"})
    attic = os.path.join(r.repo, "m", "Attic")
    os.makedirs(attic, exist_ok=True)
    shutil.copyfile(os.path.join(r.repo, "m", "a.txt,v"),
                    os.path.join(attic, "a.txt,v"))
    wc = r.checkout("m")
    check_eq(read(os.path.join(wc, "a.txt")), "one\n", "a.txt content")
    ents = entries_of(wc)
    check_eq(sorted(ents), ["a.txt", "b.txt"], "entries with an Attic duplicate")
    # An update in the working copy walks Entries plus repository plus Attic;
    # the duplicate name must still collapse to a single entry.
    _, out = r.cvs(["update"], cwd=wc)
    ents = entries_of(wc)
    check_eq(sorted(ents), ["a.txt", "b.txt"], "entries after update")


def branch_num(r, wc, fname, tag):
    """Return the magic branch revision the symbol TAG carries for FNAME."""
    _, out = r.cvs(["log", fname], cwd=wc)
    m = re.search(r"^\t%s:\s*(\S+)$" % re.escape(tag), out, re.M)
    if not check(m is not None, "%s missing from symbolic names:\n%s" % (tag, out)):
        return None
    return m.group(1)


@test("branch numbers are assigned in order and nest correctly")
def t_branch_numbers_multi(r):
    # Sequential branches must get the even magic numbers 2, 4, 6: with no
    # deletions both the scan-from-2 loop and a highest-plus-2 assignment
    # agree, so these are exact.
    r.import_tree("m", {"a.txt": "one\n"})
    wc = r.checkout("m")
    write(os.path.join(wc, "a.txt"), "one\ntwo\n")
    r.cvs(["commit", "-m", "second"], cwd=wc)          # -> 1.2
    for tag, num in (("BR1", 2), ("BR2", 4), ("BR3", 6)):
        r.cvs(["tag", "-b", tag], cwd=wc)
        check_eq(branch_num(r, wc, "a.txt", tag), "1.2.0.%d" % num,
                 "%s branch number" % tag)

    # A commit on BR1 turns the magic branch into the physical branch 1.2.2.
    r.cvs(["update", "-r", "BR1"], cwd=wc)
    write(os.path.join(wc, "a.txt"), "one\ntwo\nbr1\n")
    _, out = r.cvs(["commit", "-m", "on br1"], cwd=wc)
    check("1.2.2.1" in out, "commit on BR1 did not create 1.2.2.1:\n" + out)

    # A branch off that branch revision starts its own magic numbering at 2.
    r.cvs(["tag", "-b", "SUB1"], cwd=wc)
    check_eq(branch_num(r, wc, "a.txt", "SUB1"), "1.2.2.1.0.2",
             "SUB1 branch-off-branch number")

    # Everything must still be usable afterwards.
    r.cvs(["update", "-A"], cwd=wc)
    check_eq(read(os.path.join(wc, "a.txt")), "one\ntwo\n", "content at head")
    r.cvs(["update", "-r", "BR1"], cwd=wc)
    check_eq(read(os.path.join(wc, "a.txt")), "one\ntwo\nbr1\n", "content on BR1")
    r.cvs(["update", "-A"], cwd=wc)


@test("a branch created after deleting an unused branch stays unique")
def t_branch_after_delete_uncommitted(r):
    # Deleting BR2 (never committed on) frees magic number 4.  Whether a new
    # branch reuses the freed number or takes a fresh one is an implementation
    # choice; what must hold is that the new number is even, well-formed and
    # different from every live branch, and that the branch works.
    r.import_tree("m", {"a.txt": "one\n"})
    wc = r.checkout("m")
    write(os.path.join(wc, "a.txt"), "one\ntwo\n")
    r.cvs(["commit", "-m", "second"], cwd=wc)          # -> 1.2
    for tag in ("BR1", "BR2", "BR3"):
        r.cvs(["tag", "-b", tag], cwd=wc)
    r.cvs(["tag", "-d", "-B", "BR2"], cwd=wc)

    r.cvs(["tag", "-b", "BR4"], cwd=wc)
    num = branch_num(r, wc, "a.txt", "BR4")
    if num:
        m = re.match(r"^1\.2\.0\.(\d+)$", num)
        check(m is not None, "BR4 is not a magic branch off 1.2: %r" % num)
        if m:
            n = int(m.group(1))
            check(n % 2 == 0, "BR4 branch number %d is odd" % n)
            check(n not in (2, 6), "BR4 number %d collides with BR1/BR3" % n)

    # The branch must accept a commit that lands on its own branch number.
    r.cvs(["update", "-r", "BR4"], cwd=wc)
    write(os.path.join(wc, "a.txt"), "one\ntwo\nbr4\n")
    _, out = r.cvs(["commit", "-m", "on br4"], cwd=wc)
    if num:
        branchrev = num.replace(".0.", ".", 1) + ".1"
        check(branchrev in out,
              "commit on BR4 did not create %s:\n%s" % (branchrev, out))
    r.cvs(["update", "-A"], cwd=wc)
    check_eq(read(os.path.join(wc, "a.txt")), "one\ntwo\n", "content at head")


@test("a deleted branch with commits still blocks its branch number")
def t_branch_after_delete_committed(r):
    # BR1 gets 1.2.0.2 and a commit creates the physical branch 1.2.2.
    # Deleting the tag afterwards removes the symbol but not the branch, so
    # the next branch must skip number 2 and take 1.2.0.4: candidates are
    # validated against the delta tree, not just the symbol table.
    r.import_tree("m", {"a.txt": "one\n"})
    wc = r.checkout("m")
    write(os.path.join(wc, "a.txt"), "one\ntwo\n")
    r.cvs(["commit", "-m", "second"], cwd=wc)          # -> 1.2
    r.cvs(["tag", "-b", "BR1"], cwd=wc)
    check_eq(branch_num(r, wc, "a.txt", "BR1"), "1.2.0.2", "BR1 branch number")
    r.cvs(["update", "-r", "BR1"], cwd=wc)
    write(os.path.join(wc, "a.txt"), "one\ntwo\nbr1\n")
    r.cvs(["commit", "-m", "on br1"], cwd=wc)          # -> 1.2.2.1
    r.cvs(["update", "-A"], cwd=wc)
    r.cvs(["tag", "-d", "-B", "BR1"], cwd=wc)

    r.cvs(["tag", "-b", "BRX"], cwd=wc)
    check_eq(branch_num(r, wc, "a.txt", "BRX"), "1.2.0.4",
             "BRX must skip the committed branch 1.2.2")


@test("adding a new file writes a fully usable ,v")
def t_new_file_add_commit(r):
    # cvs add + commit of a brand-new file takes the write-a-fresh-,v path in
    # RCS_checkin (not RCS_rewrite).  The file must then survive the normal
    # rewrite paths: a second commit, a branch tag, and a branch commit.
    r.import_tree("m", {"a.txt": "one\n"})
    wc = r.checkout("m")
    payload = "".join("new file line %04d @@ at@\n" % i for i in range(200))
    write(os.path.join(wc, "n.txt"), payload)
    r.cvs(["add", "n.txt"], cwd=wc)
    _, out = r.cvs(["commit", "-m", "add n"], cwd=wc)

    wc2root = os.path.join(r.root, "wc2")
    os.makedirs(wc2root)
    r.cvs(["checkout", "m"], cwd=wc2root)
    check_eq(read(os.path.join(wc2root, "m", "n.txt")), payload,
             "new file content after fresh checkout")

    write(os.path.join(wc, "n.txt"), payload + "second\n")
    r.cvs(["commit", "-m", "second"], cwd=wc)
    r.cvs(["tag", "-b", "NBR"], cwd=wc)
    r.cvs(["update", "-r", "NBR", "n.txt"], cwd=wc)
    write(os.path.join(wc, "n.txt"), payload + "second\nbranch\n")
    r.cvs(["commit", "-m", "on branch"], cwd=wc)
    r.cvs(["update", "-A", "n.txt"], cwd=wc)
    check_eq(read(os.path.join(wc, "n.txt")), payload + "second\n",
             "new file content at head after branch commit")
    r.cvs(["update", "-r", "1.1", "n.txt"], cwd=wc)
    check_eq(read(os.path.join(wc, "n.txt")), payload, "new file revision 1.1")
    r.cvs(["update", "-A", "n.txt"], cwd=wc)


@test("a piped server session completes several commands without stalling")
def t_server_session(r):
    # Drive `cvs server` through its stdin/stdout protocol the way a network
    # client would: one session issuing valid-requests, a checkout, a noop and
    # an rlog.  Pins (a) request dispatch, (b) that every command's output is
    # flushed to the client by the time its terminating "ok" arrives, and
    # (c) that the session never deadlocks waiting for a flush.
    r.import_tree("m", {"a.txt": "one\n", "sub/b.txt": "sub content\n"})
    root = r.repo.replace(os.sep, "/")

    valid_responses = (
        "ok error Valid-requests Checked-in New-entry Checksum Copy-file "
        "Blob-ref Blob-ref-created Blob-OTP Blob-url "
        "Updated Created Update-existing Merged Patched Rcs-diff Mode "
        "Mod-time Removed Remove-entry Set-static-directory "
        "Clear-static-directory Set-sticky Clear-sticky Template "
        "Notified Module-expansion Clear-rename Rename EntriesExtra "
        "M Mbinary E F MT")
    reqs = "".join(s + "\n" for s in [
        "Root " + root,
        "Valid-responses " + valid_responses,
        "valid-requests",
        "UseUnchanged",
        "Argument m",
        "Directory .",
        root,
        "co",
        "noop",
        "Argument m",
        "rlog",
    ])

    cmd = [CVS]
    if LIBDIR:
        cmd += ["-L", LIBDIR]
    cmd += ["--allow-root=" + r.repo, "server"]
    wcdir = os.path.join(r.root, "srvwc")
    os.makedirs(wcdir)
    p = subprocess.Popen(cmd, cwd=wcdir, stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        out_b, err_b = p.communicate(reqs.encode("utf-8"), timeout=120)
    except subprocess.TimeoutExpired:
        p.kill()
        p.communicate()
        fail("server session did not complete within 120s (stalled flush?)")
        return
    out = out_b.decode("utf-8", "replace")

    check_eq(p.returncode, 0, "server exit status (output:\n%s)" % out)
    lines = out.split("\n")
    check_eq(len([l for l in lines if l == "ok"]), 4,
             "one ok per command (valid-requests, co, noop, rlog):\n" + out)
    check(not any(l.startswith("error") for l in lines),
          "server reported an error:\n" + out)
    vr = [l for l in lines if l.startswith("Valid-requests ")]
    check(vr and " co " in vr[0] and " noop " in vr[0],
          "Valid-requests line missing or incomplete:\n" + out)
    check("one\n" in out, "checked-out content of a.txt missing:\n" + out)
    check("sub content\n" in out, "checked-out content of sub/b.txt missing:\n" + out)
    # rlog output is M text; its internal line order must be preserved.
    i_rcs = out.find("M RCS file:")
    i_rev = out.find("M revision 1.1")
    i_msg = out.find("M Initial revision")
    check(0 <= i_rcs < i_rev < i_msg,
          "rlog output incomplete or reordered (%d,%d,%d):\n%s"
          % (i_rcs, i_rev, i_msg, out))
    # The checkout must have been complete by the time its ok (the second of
    # the four) arrived: nothing of the checkout may trail its terminator.
    ok_at = [i for i, l in enumerate(lines) if l == "ok"]
    content_at = [i for i, l in enumerate(lines) if l == "one" or l == "sub content"]
    if len(ok_at) >= 2 and content_at:
        check(max(content_at) < ok_at[1],
              "checkout content arrived after the co terminator:\n" + out)


@test("checkout -p sends the file body before its trailing partial line")
def t_server_pipeout_order(r):
    # `co -p` on a file that does not end in a newline emits, in order:
    #   NoTranslateBegin / M <body> / MT text <tail> / NoTranslateEnd
    # The body goes through cvs_output (staged); the tail and the
    # NoTranslate brackets are written straight to the network buffer.  If
    # staged text is not drained before those direct writes, the client
    # prints the tail before the body, and - worse - NoTranslateEnd arrives
    # first, so the client re-enables codepage translation before the body
    # and silently mistranslates it.
    body = "alpha\nbeta\ngamma no trailing newline"
    r.import_tree("m", {"p.txt": body})
    root = r.repo.replace(os.sep, "/")

    valid_responses = (
        "ok error Valid-requests Checked-in New-entry Checksum Copy-file "
        "Blob-ref Blob-ref-created Blob-OTP Blob-url "
        "Updated Created Update-existing Merged Patched Rcs-diff Mode "
        "Mod-time Removed Remove-entry Set-static-directory "
        "Clear-static-directory Set-sticky Clear-sticky Template "
        "Notified Module-expansion Clear-rename Rename EntriesExtra "
        "M Mbinary E F MT NoTranslateBegin NoTranslateEnd")
    reqs = "".join(s + "\n" for s in [
        "Root " + root,
        "Valid-responses " + valid_responses,
        "valid-requests",
        "UseUnchanged",
        "Argument -p",
        "Argument m/p.txt",
        "co",
    ])

    cmd = [CVS]
    if LIBDIR:
        cmd += ["-L", LIBDIR]
    cmd += ["--allow-root=" + r.repo, "server"]
    wcdir = os.path.join(r.root, "psrvwc")
    os.makedirs(wcdir)
    p = subprocess.Popen(cmd, cwd=wcdir, stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        out_b, _ = p.communicate(reqs.encode("utf-8"), timeout=120)
    except subprocess.TimeoutExpired:
        p.kill()
        p.communicate()
        fail("server session did not complete within 120s")
        return
    out = out_b.decode("utf-8", "replace")

    i_begin = out.find("NoTranslateBegin")
    i_body = out.find("M alpha")
    i_tail = out.find("MT text")
    i_end = out.find("NoTranslateEnd")

    if i_begin < 0 or i_end < 0:
        # The server only emits the brackets when the client advertises them;
        # if this build does not, there is nothing to order.
        check(i_body >= 0, "file body missing entirely:\n" + out)
        return

    check(0 <= i_begin < i_body,
          "body arrived before NoTranslateBegin (%d,%d):\n%s" % (i_begin, i_body, out))
    check(0 <= i_body < i_end,
          "NoTranslateEnd arrived before the file body (%d,%d) - the client "
          "would re-enable codepage translation before receiving it:\n%s"
          % (i_body, i_end, out))
    if i_tail >= 0:
        check(i_body < i_tail,
              "trailing partial line arrived before the body (%d,%d):\n%s"
              % (i_body, i_tail, out))


@test("a watched file checks out read-only, an unwatched one writable")
def t_watched_readonly(r):
    # "Is this file watched?" is answered per checked-out file from the
    # repository's CVS/fileattr.xml.  The lookup must be per file (w.txt
    # watched, n.txt not) and must use fncmp semantics: on Windows a
    # case-variant name attribute still matches.
    r.import_tree("m", {"w.txt": "watched\n", "n.txt": "not watched\n"})
    attrdir = os.path.join(r.repo, "m", "CVS")
    attr = os.path.join(attrdir, "fileattr.xml")

    def fresh_checkout(sub):
        wcroot = os.path.join(r.root, sub)
        os.makedirs(wcroot)
        r.cvs(["checkout", "m"], cwd=wcroot)
        return os.path.join(wcroot, "m")

    write(attr, '<?xml version="1.0"?>\n<fileattr>\n'
                '  <file name="w.txt">\n    <watched/>\n  </file>\n'
                '</fileattr>\n')
    wc = fresh_checkout("wcA")
    check(not os.access(os.path.join(wc, "w.txt"), os.W_OK),
          "watched w.txt checked out writable")
    check(os.access(os.path.join(wc, "n.txt"), os.W_OK),
          "unwatched n.txt checked out read-only")

    # The update path answers the same question when re-creating a file.
    os.chmod(os.path.join(wc, "w.txt"), 0o644)
    os.remove(os.path.join(wc, "w.txt"))
    r.cvs(["update"], cwd=wc)
    check(not os.access(os.path.join(wc, "w.txt"), os.W_OK),
          "watched w.txt restored writable by update")

    if sys.platform == "win32":
        write(attr, '<?xml version="1.0"?>\n<fileattr>\n'
                    '  <file name="W.TXT">\n    <watched/>\n  </file>\n'
                    '</fileattr>\n')
        wc = fresh_checkout("wcB")
        check(not os.access(os.path.join(wc, "w.txt"), os.W_OK),
              "case-variant watched name not honoured on Windows")

    os.remove(attr)
    wc = fresh_checkout("wcC")
    check(os.access(os.path.join(wc, "w.txt"), os.W_OK),
          "w.txt still read-only after the watch attribute was removed")


@test("repository operations append well-formed history records")
def t_history_records(r):
    # History logging is enabled by the presence of CVSROOT/history (a full
    # cvs init creates it; init -n does not, so create it the way an admin
    # enabling logging would).  checkout, commit and tag must each append
    # records, and every record must parse.
    hist = os.path.join(r.repo, "CVSROOT", "history")
    write(hist, "")
    files = dict(("f%02d.txt" % i, "content %d\n" % i) for i in range(12))
    r.import_tree("m", files)
    n0 = len(read(hist).splitlines())

    wc = r.checkout("m")
    n1 = len(read(hist).splitlines())
    check(n1 > n0, "checkout appended no history record (%d -> %d)" % (n0, n1))

    write(os.path.join(wc, "f00.txt"), "changed\n")
    r.cvs(["commit", "-m", "change"], cwd=wc)
    n2 = len(read(hist).splitlines())
    check(n2 > n1, "commit appended no history record (%d -> %d)" % (n1, n2))

    r.cvs(["tag", "HT1"], cwd=wc)
    n3 = len(read(hist).splitlines())
    check(n3 > n2, "tag appended no history record (%d -> %d)" % (n2, n3))

    for line in read(hist).splitlines():
        if not line.strip():
            continue
        check(re.match(r"^[A-Za-z][0-9a-f]{8,16}\|[^|]*\|", line) is not None,
              "malformed history record: %r" % line)

    _, out = r.cvs(["history", "-e", "-a"], cwd=wc)
    check("f00.txt" in out or "m" in out,
          "cvs history reports nothing for the recorded operations:\n" + out)


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

@test("small -kB binary second revision checks out byte for byte")
def t_binary_small_second_commit(r):
    # Same shape as t_binary_second_commit, but a small payload.  Binary
    # content used to pass through the codepage encoder, which guessed an
    # encoding from the bytes; a wrong guess and a failed iconv wrote the
    # working file out empty, and only payloads the guesser left alone
    # (like the 1541-byte one above) survived.  Several sizes, so a lucky
    # guess on one of them cannot hide a regression.
    payload1 = bytes(range(256))
    imp = os.path.join(r.root, "impbin")
    os.makedirs(imp)
    with open(os.path.join(imp, "b.dat"), "wb") as f:
        f.write(payload1)
    r.cvs(["import", "-m", "bin", "-kb", "mb", "VENDOR", "REL0"], cwd=imp)
    wc = r.checkout("mb")
    # GuessEncoding keys on the leading bytes and on length parity, so the
    # cases vary both: no BOM at even and odd lengths, the UCS-2LE and
    # UCS-2BE guesses, a UTF-8 BOM, and the odd 1541-byte shape that used
    # to dodge the defect - each takes its own branch.
    base = bytes(range(256)) * 7
    cases = ((100, bytes()), (301, bytes()), (768, bytes([255, 254])),
             (1200, bytes([254, 255])), (1401, bytes([239, 187, 191])),
             (1541, bytes([255, 254])))
    for n, (size, prefix) in enumerate(cases):
        payload = (prefix + base)[:size]
        with open(os.path.join(wc, "b.dat"), "wb") as f:
            f.write(payload)
        r.cvs(["commit", "-m", "rev %d" % n], cwd=wc)
        fresh = os.path.join(r.root, "fresh%d" % n)
        os.makedirs(fresh)
        r.cvs(["checkout", "mb"], cwd=fresh)
        got = open(os.path.join(fresh, "mb", "b.dat"), "rb").read()
        check_eq(len(got), size,
                 "%d-byte binary revision came back as %d bytes" % (size, len(got)))
        check(got == payload, "%d-byte binary revision content differs" % size)

@test("binary content is detected on add and import by content, not by name")
def t_add_binary_by_content(r):
    # A NUL in the first 8000 bytes makes a file binary whatever its name
    # or cvswrappers say; UTF-16 text with a BOM is exempt.  An explicit
    # text -k on such a file is refused.  import follows the same rule.
    r.import_tree("m", {"a.txt": "one" + chr(10)})
    wc = r.checkout("m")
    nul = bytes(range(256)) * 3
    with open(os.path.join(wc, "blob1.txt"), "wb") as f:
        f.write(nul)
    # Wrappers may still opt a text file into -kB by name (*.bin is one);
    # content only ever adds binary-ness.  So the text case has no
    # extension at all.
    write(os.path.join(wc, "plain_text"), "just text" + chr(10))
    with open(os.path.join(wc, "u16.txt"), "wb") as f:
        f.write(bytes([255, 254]) + "hello".encode("utf-16-le"))
    _, out = r.cvs(["add", "blob1.txt", "plain_text", "u16.txt"], cwd=wc)
    ents = entries_of(wc)
    check("-kB" in ents.get("blob1.txt", ""),
          "NUL-bearing blob1.txt not added as -kB: " + ents.get("blob1.txt", "<absent>"))
    check("-k" not in ents.get("plain_text", "/x/"),
          "text plain_text got a kopt: " + ents.get("plain_text", "<absent>"))
    check("-kB" not in ents.get("u16.txt", ""),
          "UTF-16 text u16.txt treated as binary: " + ents.get("u16.txt", "<absent>"))
    check("blob1.txt has binary content" in out, "no note about the auto -kB:" + chr(10) + out)

    with open(os.path.join(wc, "blob2.txt"), "wb") as f:
        f.write(nul)
    write(os.path.join(wc, "plain2"), "text too" + chr(10))
    rc, out = r.cvs(["add", "-kkv", "blob2.txt", "plain2"], cwd=wc, expect_ok=False)
    check(rc != 0, "add -kkv on binary content exited 0:" + chr(10) + out)
    # The refusal covers the whole command: nothing is registered, not even
    # the text file named alongside.
    check("blob2.txt" not in entries_of(wc), "add -kkv registered binary content as text")
    check("plain2" not in entries_of(wc), "add -kkv registered the text sibling of a refused file")
    # A +B delta already says binary and must be accepted as such.
    with open(os.path.join(wc, "blob4.txt"), "wb") as f:
        f.write(nul)
    rc, out = r.cvs(["add", "-k+B", "blob4.txt"], cwd=wc, expect_ok=False)
    check_eq(rc, 0, "add -k+B on binary content was refused:" + chr(10) + out)
    check("B" in entries_of(wc).get("blob4.txt", "").split("/")[4],
          "add -k+B did not register binary: " + entries_of(wc).get("blob4.txt", "<absent>"))
    # One add spanning two directories keeps each file's own verdict.
    os.makedirs(os.path.join(wc, "sub"))
    r.cvs(["add", "sub"], cwd=wc)
    with open(os.path.join(wc, "sub", "deep.txt"), "wb") as f:
        f.write(nul)
    write(os.path.join(wc, "top_text"), "top" + chr(10))
    r.cvs(["add", "top_text", os.path.join("sub", "deep.txt")], cwd=wc)
    check("-kB" in entries_of(os.path.join(wc, "sub")).get("deep.txt", ""),
          "binary file in a second directory lost its -kB: "
          + entries_of(os.path.join(wc, "sub")).get("deep.txt", "<absent>"))
    check("-k" not in entries_of(wc).get("top_text", "/x/"),
          "text file in the first directory got a kopt: " + entries_of(wc).get("top_text", "<absent>"))

    r.cvs(["commit", "-m", "bin"], cwd=wc)
    wcb = os.path.join(r.root, "wcBin")
    os.makedirs(wcb)
    r.cvs(["checkout", "m"], cwd=wcb)
    got = open(os.path.join(wcb, "m", "blob1.txt"), "rb").read()
    check_eq(got, nul, "auto -kB file did not round trip")

    imp = os.path.join(r.root, "impb")
    os.makedirs(imp)
    with open(os.path.join(imp, "blob.txt"), "wb") as f:
        f.write(nul)
    write(os.path.join(imp, "notes.dat"), "text" + chr(10))
    r.cvs(["import", "-m", "i", "mi", "VENDOR", "REL0"], cwd=imp)
    wci = r.checkout("mi")
    e = entries_of(wci)
    check("-kB" in e.get("blob.txt", ""),
          "imported NUL-bearing blob.txt not -kB: " + e.get("blob.txt", "<absent>"))
    check("-k" not in e.get("notes.dat", "/x/"),
          "imported text notes.dat got a kopt: " + e.get("notes.dat", "<absent>"))
    check_eq(open(os.path.join(wci, "blob.txt"), "rb").read(), nul, "imported blob.txt content")

    impk = os.path.join(r.root, "impk")
    os.makedirs(impk)
    with open(os.path.join(impk, "blobk.txt"), "wb") as f:
        f.write(nul)
    rc, out = r.cvs(["import", "-k", "kv", "-m", "i", "mk", "VENDOR", "REL0"], cwd=impk,
                    expect_ok=False)
    check(rc != 0, "import -k kv on binary content exited 0:" + chr(10) + out)
    check(not os.path.exists(os.path.join(r.repo, "mk", "blobk.txt,v")),
          "import -k kv stored binary content as text")


@test("a per-file Kopt before Is-modified makes the server add that file as -kB")
def t_add_binary_kopt_protocol(r):
    # The client-side detector tags a binary file with "Kopt -kB" followed
    # by "Is-modified", which the server turns into a dummy entry carrying
    # the kopt, so one add can mix text and binary.  No remote method is
    # buildable here without admin rights, so this drives cvs server over
    # its protocol with the exact sequence the client emits and pins the
    # server half of that contract.
    r.import_tree("m", {"a.txt": "one" + chr(10), "sub/b.txt": "two" + chr(10)})
    root = r.repo.replace(os.sep, "/")
    valid_responses = (
        "ok error Valid-requests Checked-in New-entry Checksum Copy-file "
        "Blob-ref Blob-ref-created Blob-OTP Blob-url "
        "Updated Created Update-existing Merged Patched Rcs-diff Mode "
        "Mod-time Removed Remove-entry Set-static-directory "
        "Clear-static-directory Set-sticky Clear-sticky Template "
        "Notified Module-expansion Clear-rename Rename EntriesExtra "
        "M Mbinary E F MT")
    reqs = "".join(s + chr(10) for s in [
        "Root " + root,
        "Valid-responses " + valid_responses,
        "valid-requests",
        "UseUnchanged",
        "Directory .",
        root + "/m",
        "Kopt -kB",
        "Is-modified blob1.txt",
        "Is-modified plain_text",
        "Directory sub",
        root + "/m/sub",
        "Kopt -kB",
        "Is-modified deep.txt",
        # A real client ends the walk back at the top; the server runs the
        # command from the last Directory it was given.
        "Directory .",
        root + "/m",
        "Argument --",
        "Argument blob1.txt",
        "Argument plain_text",
        "Argument sub/deep.txt",
        "add",
    ])
    cmd = [CVS]
    if LIBDIR:
        cmd += ["-L", LIBDIR]
    cmd += ["--allow-root=" + r.repo, "server"]
    wcdir = os.path.join(r.root, "srvadd")
    os.makedirs(wcdir)
    p = subprocess.Popen(cmd, cwd=wcdir, stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        out_b, err_b = p.communicate(reqs.encode("utf-8"), timeout=120)
    except subprocess.TimeoutExpired:
        p.kill()
        p.communicate()
        fail("server add did not complete within 120s")
        return
    out = out_b.decode("utf-8", "replace") + err_b.decode("utf-8", "replace")
    check_eq(p.returncode, 0, "server exit status (output:" + chr(10) + out + ")")
    check(not any(l.startswith("error") for l in out.split(chr(10))),
          "server reported an error:" + chr(10) + out)
    # In server mode the entry carries no timestamp, and a client that has
    # not sent Valid-RcsOptions is answered with the compatibility spelling
    # -kb (the kflag table maps B to b for pre-cvsnt clients).
    check(re.search(r"/blob1\.txt/0/[^/]*/-k[bB]/", out) is not None,
          "Kopt -kB before Is-modified did not make the added entry binary:" + chr(10) + out)
    check(re.search(r"/plain_text/0/[^/]*//", out) is not None,
          "the file without a Kopt in the same add did not stay text:" + chr(10) + out)
    # The second directory's file keeps its own Kopt: one Is-modified per file,
    # sent inside its Directory, survives the flush a directory change causes.
    check(re.search(r"/deep\.txt/0/[^/]*/-k[bB]/", out) is not None,
          "the binary file in the second directory lost its Kopt:" + chr(10) + out)



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
        failed_now = len(FAILURES) > before
        xreason = getattr(fn, "_xfail", None)
        if xreason:
            del FAILURES[before:]
            if failed_now:
                XFAILED.append(CURRENT)
                print("  xfail " + CURRENT)
            else:
                fail("XPASS: expected to fail but passed - remove the xfail marker (%s)" % xreason)
                print("  XPASS " + CURRENT)
        elif not failed_now:
            PASSED += 1
            print("  ok    " + CURRENT)
        else:
            print("  FAIL  " + CURRENT)
            for _, msg in FAILURES[before:]:
                print("          " + msg.replace("\n", "\n          "))

    print()
    xf = len(XFAILED)
    print("%d passed, %d failed%s" % (
          PASSED, len(tests) - PASSED - xf,
          (", %d xfail" % xf) if xf else ""))

    if not args.keep:
        shutil.rmtree(scratch, ignore_errors=True)
    else:
        print("kept: " + scratch)

    return 0 if not FAILURES else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env bash
# Assert that the suites ran to completion and that the cases behind the
# open PRs actually executed instead of being skipped.
#
#   check_suite_logs.sh <regress.log> <testcvs.log> [regress.py]
#
# With regress.py given, a required case that this tree's regress.py does
# not define (its PR is not part of the tree) is reported, not failed.
#
# regress.py prints "ok <name>" per passing test and a "<n> passed, <m> failed"
# summary; the -ku case prints a "(skipped: ...)" note and still counts as ok
# when the ext protocol plugin is missing, which is exactly the silent
# failure this guards against. testcvs.py exits 0 even on failure (it raises
# a bare SystemExit), so its result has to be read from the log: a failure
# prints "Test '...' failed (" or "Terminating", success reaches the last
# scenario, "*info".
set -u
regress="$1"
testcvs="$2"
suite="${3:-}"
rc=0

fail() { echo "::error::$*"; rc=1; }

echo "--- regress.py"
grep -E '^[0-9]+ passed, 0 failed' "$regress" >/dev/null || fail "regress.py did not report zero failures"
grep -E '^\s+FAIL\s' "$regress" && fail "regress.py has FAIL lines"
# regress.py has exactly two skip notes (grep -n skipped regress.py):
#   "(skipped: the :ext: protocol plugin is not available here)"  - the -ku
#       case could not open a client/server session; this is the silent
#       failure CI exists to catch, so it fails the job
#   "(symlink case skipped: not permitted here)"  - a sub-case of the
#       binary-by-content test that needs SeCreateSymbolicLinkPrivilege; a
#       windows-2022 runner does not grant it, the rest of the test still
#       runs, and the Linux job covers the symlink path. Allowed.
# Anything else mentioning a skipped protocol, plugin or fork is new and
# fails until it is reviewed.
if grep -v 'symlink case skipped' "$regress" | grep -Ei 'skipped.*(protocol|plugin|fork)|(protocol|plugin|fork).*skipped' ; then
  fail "regress.py skipped a protocol case; every client/server case must execute in CI"
fi
for t in "a -ku text file checks out with every line ending encoded" \
         "binary content is detected on add and import by content, not by name" \
         "binary file survives a commit/checkout round trip byte for byte"; do
  if [ -n "$suite" ] && ! grep -F "@test(\"$t\")" "$suite" >/dev/null; then
    echo "::notice::regress.py: '$t' is not defined in this tree, not required"
    continue
  fi
  grep -F "  ok    $t" "$regress" >/dev/null || fail "regress.py: '$t' did not pass"
done

echo "--- testcvs.py"
grep -E "failed \(|Terminating" "$testcvs" && fail "testcvs.py reported a failure"
grep -Fx '*info' "$testcvs" >/dev/null || fail "testcvs.py did not reach its last scenario (*info)"
for t in "Basic Add, Remove, Resurrect, Commit" "Basic binary Add/Checkout" "Binary remove and revert"; do
  grep -Fx "$t" "$testcvs" >/dev/null || fail "testcvs.py: scenario '$t' did not run"
done

[ $rc -eq 0 ] && echo "suite logs OK"
exit $rc

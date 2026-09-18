#!/usr/bin/env bash
set -u
regress="$1"
testcvs="$2"
suite="${3:-}"
rc=0

fail() { echo "::error::$*"; rc=1; }

echo "--- regress.py"
grep -E '^[0-9]+ passed, 0 failed' "$regress" >/dev/null || fail "regress.py did not report zero failures"
grep -E '^\s+FAIL\s' "$regress" && fail "regress.py has FAIL lines"
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

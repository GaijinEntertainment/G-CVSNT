#!/usr/bin/env bash
set -eu
PREFIX=${PREFIX:-/usr/local/cvsnt}
SRC=${SRC:-/src}
WORK=${WORK:-/work}
EXPECTED_BUILD=${EXPECTED_BUILD:-}

export PATH="$PREFIX/bin:$PATH" CVS_DIR="$PREFIX/bin" HOME="$WORK"
mkdir -p "$WORK"
cd "$WORK"

cvs --version
if [ -n "$EXPECTED_BUILD" ]; then
  cvs --version | grep -F "Build $EXPECTED_BUILD" >/dev/null || {
    echo "::error::cvs --version does not report Build $EXPECTED_BUILD"; exit 1; }
fi
ls "$PREFIX/lib/cvsnt/protocols/ext.so" >/dev/null

if [ ! -f "$SRC/testcvs/regress.py" ]; then
  echo "::error::$SRC/testcvs/regress.py not found — this tree has no test suites yet (they arrive with an audit branch, e.g. audit/02-analysis-reports-and-fixes); pass a base/prs combination that includes testcvs/"
  exit 1
fi

cvslockd
sleep 1

echo "=== regress.py"
python3.9 "$SRC/testcvs/regress.py" --cvs "$PREFIX/bin/cvs" --libdir "$PREFIX/lib/cvsnt" 2>&1 | tee regress.log
test "${PIPESTATUS[0]}" -eq 0

echo "=== testcvs.py"
rm -rf suite && cp -r "$SRC/testcvs" suite && cd suite
python3.9 testcvs.py 2>&1 | tee ../testcvs.log
cd ..

bash "$(dirname "$0")/check_suite_logs.sh" regress.log testcvs.log "$SRC/testcvs/regress.py"

#!/usr/bin/env bash
set -euo pipefail
CVS="$1"
W="$2"
CVS_TIMEOUT=${CVS_TIMEOUT:-120}
cvs_cmd() {
  timeout "$CVS_TIMEOUT" "$CVS" "$@"
  local rc=$?
  [ $rc -eq 124 ] && echo "::error::cvs $* hung for ${CVS_TIMEOUT}s and was killed"
  return $rc
}
ROOT=${ROOT:-":pserver:cvs:cvs@127.0.0.1:2401/cvs"}

rm -rf "$W"; mkdir -p "$W/imp/sub/deep"
cd "$W"

# The total comes out of the script itself, so adding a scenario cannot leave
# the count behind. A `?` means the file could not be read, not zero scenarios.
STEP_TOTAL=$(grep -c '^step "' "$0" 2>/dev/null) || STEP_TOTAL=0
[ "$STEP_TOTAL" -gt 0 ] 2>/dev/null || STEP_TOTAL='?'
STEP_DONE=0
STEP_NAME='(none)'
FINISHED=0
step() {
  STEP_DONE=$(( STEP_DONE + 1 ))
  STEP_NAME="$*"
  echo
  echo "### [$STEP_DONE/$STEP_TOTAL] $*"
}

# set -e aborts on the first failed check, so without this the log ends on
# whatever command failed and never says how far the scenario list got.
on_exit() {
  [ "$FINISHED" = 1 ] && return 0
  echo
  echo "### SMOKE FAILED in scenario $STEP_DONE of $STEP_TOTAL: $STEP_NAME"
  echo "###   $(( STEP_DONE - 1 )) scenarios passed before it"
  return 0
}
trap on_exit EXIT

cmp_text() { diff <(tr -d '\r' < "$1") <(tr -d '\r' < "$2") > /dev/null; }
same_bytes() {  # <expected file> <file under test> <what it proves>
  if ! cmp -s "$1" "$2"; then
    echo "::error::$3: expected $(stat -c %s "$1") bytes, got $(stat -c %s "$2" 2>/dev/null || echo 0)"
    exit 1
  fi
  echo "    $3"
}
hash_tree() { (cd "$1" && find . -type f -not -path '*/CVS/*' | LC_ALL=C sort | xargs -r sha256sum); }
SOURCE_MISMATCH=0
compare_with_source() {
  local src="$1" wc="$2" f rel msg
  while IFS= read -r f; do
    rel=${f#"$src"/}
    msg=""
    case "$rel" in
      *.txt) cmp_text "$f" "$wc/$rel" || msg="$rel: text differs from the source" ;;
      *)     cmp "$f" "$wc/$rel" || msg="$rel: binary differs from the source ($(stat -c %s "$f") vs $(stat -c %s "$wc/$rel" 2>/dev/null || echo 0) bytes)" ;;
    esac
    [ -z "$msg" ] && continue
    echo "::error::$msg"
    SOURCE_MISMATCH=1
  done < <(find "$src" -type f -not -path '*/CVS/*' | LC_ALL=C sort)
  echo "    compared with the source: $src"
  return 0
}
mkbin() { { printf '\000\377'; head -c $(( $2 - 2 )) /dev/zero | tr '\0' 'x'; } > "$1"; }
sync_and_compare() {
  (cd wc2 && cvs_cmd -Q update -dP)
  if ! diff <(hash_tree proj) <(hash_tree wc2); then
    echo "::error::working copies differ after: $*"; exit 1
  fi
  echo "    trees identical after: $*"
}

printf 'one\ntwo\n'            > imp/a.txt
printf 'deep\n'                > imp/sub/deep/d.txt
mkbin imp/b.dat 5000
mkbin imp/sub/l.dat 200000

step "version"
cvs_cmd -d "$ROOT" version

step "import"
(cd imp && cvs_cmd -d "$ROOT" import -m init proj VENDOR REL0)

step "checkout, twice; the checkout must equal the import source"
cvs_cmd -d "$ROOT" checkout proj
test -f proj/a.txt && test -f proj/sub/deep/d.txt
compare_with_source imp proj
cvs_cmd -d "$ROOT" checkout -d wc2 proj
sync_and_compare "checkout"

step "mixed add of a text and a binary + commit (blob push through cafs_server), update"
mkdir -p added
printf 'added\n' > added/c.txt
head -c 300000 /dev/urandom > added/n.dat
cp added/c.txt added/n.dat proj/
(cd proj && cvs_cmd add c.txt && cvs_cmd add -kb n.dat && cvs_cmd commit -m "add text and binary")
sync_and_compare "mixed add + commit"
compare_with_source added wc2

step "modify + commit, update"
printf 'one\ntwo\nthree\n' > proj/a.txt
(cd proj && cvs_cmd commit -m "modify a")
sync_and_compare "modify + commit"
cmp_text proj/a.txt wc2/a.txt

step "second revision of the binary, update"
head -c 200000 /dev/urandom > proj/n.dat
(cd proj && cvs_cmd commit -m "modify binary")
sync_and_compare "binary modify + commit"
cmp proj/n.dat wc2/n.dat

step "remove + commit, update"
(cd proj && cvs_cmd remove -f c.txt && cvs_cmd commit -m "remove c")
sync_and_compare "remove + commit"
test ! -f wc2/c.txt

step "remove of the binary + commit, update"
(cd proj && cvs_cmd remove -f n.dat && cvs_cmd commit -m "remove binary")
sync_and_compare "binary remove + commit"
test ! -f wc2/n.dat

step "tag / log / status / history"
(cd proj && cvs_cmd tag SMOKE_TAG)
(cd proj && cvs_cmd log a.txt > ../log.out && head -3 ../log.out)
(cd proj && cvs_cmd status a.txt > ../status.out && head -3 ../status.out)
cvs_cmd -d "$ROOT" history -a -x MAR > history.out 2>&1 || true
head -3 history.out

step "checkout by tag"
cvs_cmd -d "$ROOT" checkout -d wc3 -r SMOKE_TAG proj
cmp_text proj/a.txt wc3/a.txt

step "bulk add + commit of 400 files (>= 8 KiB output batching), update"
for i in $(seq 1 400); do printf 'line %d\n' "$i" > "proj/f$i.txt"; done
(cd proj && cvs_cmd -Q add $(seq -f 'f%g.txt' 1 400) && cvs_cmd -Q commit -m "bulk 400")
sync_and_compare "bulk add + commit"
test "$(ls wc2/f*.txt | wc -l)" = "400"

step "branch switch back and forth"
cvs_cmd -d "$ROOT" checkout -d wc4 proj
(cd wc4 && cvs_cmd update -dP -r SMOKE_TAG)
test ! -f wc4/f1.txt
(cd wc4 && cvs_cmd update -dP -A)
test -f wc4/f1.txt

step "an independent checkout equals the updated working copy"
diff <(hash_tree wc2) <(hash_tree wc4)

# From here on everything works in working copies of its own, so the tree
# comparison above keeps comparing the trees it was written to compare.
#
# These scenarios exist against the assembled stack because their local-mode
# counterparts in testcvs.py and regress.py cannot answer for it: local mode
# never points the blob store at the repository, so it exercises a code path no
# client here uses. This is the same ground over the protocol people work on.

step "binary kflags end to end: -kb and -kB survive a fresh checkout"
# A fresh checkout, not an update: it proves the bytes came back out of the
# repository and the blob store, not out of a working copy that still held them.
cvs_cmd -d "$ROOT" checkout -d wc5 proj
mkdir -p bin1
head -c 120000 /dev/urandom > bin1/plain_bin.dat
head -c 120000 /dev/urandom > bin1/delta_bin.dat
# The names differ by more than case: names that differ only in case are one
# file on a Windows working copy, and this scenario is meant to port there.
cp bin1/plain_bin.dat bin1/delta_bin.dat wc5/
(cd wc5 && cvs_cmd add -kb plain_bin.dat && cvs_cmd add -kB delta_bin.dat \
        && cvs_cmd commit -m "add -kb and -kB")
cvs_cmd -d "$ROOT" checkout -d wc6 proj
same_bytes bin1/plain_bin.dat wc6/plain_bin.dat "-kb 1.1 checks out byte for byte"
same_bytes bin1/delta_bin.dat wc6/delta_bin.dat "-kB 1.1 checks out byte for byte"

step "second revision of each binary, fresh checkout"
mkdir -p bin2
head -c 90000 /dev/urandom > bin2/plain_bin.dat
head -c 90000 /dev/urandom > bin2/delta_bin.dat
cp bin2/plain_bin.dat bin2/delta_bin.dat wc5/
(cd wc5 && cvs_cmd commit -m "second revision of both binaries")
cvs_cmd -d "$ROOT" checkout -d wc7 proj
same_bytes bin2/plain_bin.dat wc7/plain_bin.dat "-kb 1.2 checks out byte for byte"
same_bytes bin2/delta_bin.dat wc7/delta_bin.dat "-kB 1.2 checks out byte for byte"

step "an older binary revision, and back to the head"
(cd wc5 && cvs_cmd update -r 1.1 plain_bin.dat delta_bin.dat)
same_bytes bin1/plain_bin.dat wc5/plain_bin.dat "-kb update -r 1.1"
same_bytes bin1/delta_bin.dat wc5/delta_bin.dat "-kB update -r 1.1"
(cd wc5 && cvs_cmd update -A plain_bin.dat delta_bin.dat)
same_bytes bin2/plain_bin.dat wc5/plain_bin.dat "-kb update -A"
same_bytes bin2/delta_bin.dat wc5/delta_bin.dat "-kB update -A"

step "sticky tag on a binary"
(cd wc5 && cvs_cmd tag SMOKE_BIN_TAG plain_bin.dat)
mkdir -p bin3
head -c 70000 /dev/urandom > bin3/plain_bin.dat
cp bin3/plain_bin.dat wc5/
(cd wc5 && cvs_cmd commit -m "third revision of plain_bin.dat")
(cd wc5 && cvs_cmd update -r SMOKE_BIN_TAG plain_bin.dat)
same_bytes bin2/plain_bin.dat wc5/plain_bin.dat "the tag still resolves to 1.2"
(cd wc5 && cvs_cmd update -A plain_bin.dat)
same_bytes bin3/plain_bin.dat wc5/plain_bin.dat "update -A returns to 1.3"

step "remove a binary, then check out an older revision of it"
(cd wc5 && cvs_cmd remove -f delta_bin.dat && cvs_cmd commit -m "remove delta_bin.dat")
if [ -f wc5/delta_bin.dat ]; then
  echo "::error::delta_bin.dat is still in the working copy after remove + commit"; exit 1
fi
(cd wc5 && cvs_cmd update -r 1.1 delta_bin.dat)
same_bytes bin1/delta_bin.dat wc5/delta_bin.dat "a removed binary still checks out at 1.1"

step "branch and merge over :pserver:"
printf 'base1\nbase2\nbase3\n' > wc5/merge_src.txt
(cd wc5 && cvs_cmd add merge_src.txt && cvs_cmd commit -m "base for the merge")
(cd wc5 && cvs_cmd tag -b SMOKE_MERGE_BRANCH merge_src.txt \
        && cvs_cmd update -r SMOKE_MERGE_BRANCH merge_src.txt)
printf 'base1\nbase2\nbase3\nfrom-branch\n' > wc5/merge_src.txt
(cd wc5 && cvs_cmd commit -m "branch change")
(cd wc5 && cvs_cmd update -A merge_src.txt)
printf 'from-trunk\nbase1\nbase2\nbase3\n' > wc5/merge_src.txt
(cd wc5 && cvs_cmd commit -m "trunk change")
(cd wc5 && cvs_cmd update -j SMOKE_MERGE_BRANCH merge_src.txt)
merged=$(tr -d '\r' < wc5/merge_src.txt)
for line in from-trunk from-branch; do
  case "$merged" in
    *"$line"*) ;;
    *) echo "::error::the merge lost $line"; exit 1 ;;
  esac
done
echo "    the merge carries both the trunk and the branch change"

FINISHED=1
echo
if [ "$SOURCE_MISMATCH" -ne 0 ]; then
  echo "### SMOKE FAILED: a working copy differed from its source (see ::error:: above)"
  echo "###   all $STEP_DONE of $STEP_TOTAL scenarios ran, but a comparison did not hold"
  exit 1
fi
echo "### $STEP_DONE of $STEP_TOTAL scenarios passed"
echo "### SMOKE OK"

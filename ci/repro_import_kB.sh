#!/bin/bash
set -u
export MSYS_NO_PATHCONV=1
CVS="$1"
ROOT="$2"
W="$3"
KOPT="${4:-}"

MOD="repro_import_kb_$(date +%Y%m%d%H%M%S)_$$"
rm -rf "$W"; mkdir -p "$W/src"
cd "$W"

mkfile() {
    printf '\000\377' > "src/$1"
    head -c $(( $2 - 2 )) /dev/zero | tr '\0' 'x' >> "src/$1"
}
mkfile s.dat 8
mkfile m.dat 5000
mkfile l.dat 200000

(cd src && "$CVS" -d "$ROOT" import $KOPT -m repro "$MOD" VENDOR REL0) > import.log 2>&1
echo "import rc=$? module=$MOD"
"$CVS" -d "$ROOT" checkout -d co "$MOD" > checkout.log 2>&1

rc=0
for f in s.dat m.dat l.dat; do
    kopt=$("$CVS" -d "$ROOT" rlog -h "$MOD/$f" 2>/dev/null | sed -n 's/^keyword substitution: //p')
    src=$(stat -c %s "src/$f")
    co=$(stat -c %s "co/$f" 2>/dev/null || echo 0)
    if cmp -s "src/$f" "co/$f"; then res=OK; else res=DIFFER; rc=1; fi
    echo "$f src=$src co=$co kopt=${kopt:-?} $res"
done
exit $rc

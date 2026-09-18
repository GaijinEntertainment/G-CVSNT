#!/usr/bin/env bash
# Bring the contour up with a fresh repository.
#
#   up.sh <conf dir>       writes PServer (+ generated BlobOTP) and Plugins there
#
# Exports for the caller via $GITHUB_ENV when present: CONTOUR_CONF,
# CAFS_SECRET. Requires CVSNT_IMAGE_PREFIX/CVSNT_IMAGE_TAG in the environment
# (defaults: cvsnt / ci).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
CONF=${1:?conf dir}

mkdir -p "$CONF"
export CAFS_SECRET=${CAFS_SECRET:-$(python3 -c 'import secrets; print(secrets.token_hex(24))')}
export CONTOUR_CONF="$CONF"
{ cat "$HERE/PServer.in"; printf 'BlobOTP=%s\n' "$CAFS_SECRET"; } > "$CONF/PServer"
: > "$CONF/Plugins"
if [ -n "${GITHUB_ENV:-}" ]; then
  # the secret is per run and only reaches the containers; mask it anyway
  echo "::add-mask::$CAFS_SECRET"
  { echo "CONTOUR_CONF=$CONTOUR_CONF"; echo "CAFS_SECRET=$CAFS_SECRET"; } >> "$GITHUB_ENV"
fi

cd "$HERE"
docker compose up -d

# The volume is created by root; init must run as cvs (52) and with -n,
# because the repository is already declared in PServer. blobs/ is created
# by nothing else, and without it every binary commit fails with
# "Can't send binary blob data".
docker compose exec -T --user 0:0 authserver sh -c 'mkdir -p /data/repos/cvs && chown -R 52:52 /data/repos'
docker compose exec -T --user 52:52 authserver sh -c '
  export PATH=/usr/local/cvsnt/bin:$PATH CVS_DIR=/usr/local/cvsnt/bin HOME=/tmp
  cvs -d /data/repos/cvs init -n && mkdir -p /data/repos/cvs/blobs'

accepts() {
  for _ in $(seq 30); do
    (exec 3<>"/dev/tcp/127.0.0.1/$1") 2>/dev/null && return 0
    sleep 1
  done
  echo "::error::nothing accepts connections on 127.0.0.1:$1"
  docker compose logs
  return 1
}
accepts 2401
accepts 2403
docker compose ps

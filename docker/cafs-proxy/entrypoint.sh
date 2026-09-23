#!/bin/bash
# cafs_proxy_server master_url cache_folder [validate_blobs_from_master] [update_mtime_on_access] [encryption|mandatory_encryption secret] [cache_soft_limit_size_mb]
#
# BLOB_OTP is the shared secret; SECRET is accepted as an alias.
# ENCRYPTION=none runs without a secret; the proxy then refuses clients that authenticate.
set -eu
: "${MASTER_URL:?set MASTER_URL (host name of the cafs server; port is always 2403)}"
: "${CACHE_FOLDER:=/var/cache/cafs}"
: "${VALIDATE_BLOBS:=}"
: "${UPDATE_MTIME:=}"
: "${ENCRYPTION:=mandatory_encryption}"
: "${BLOB_OTP:=${SECRET:-}}"
case "$ENCRYPTION" in
  none)
    [ -z "$BLOB_OTP" ] || echo "ENCRYPTION=none: BLOB_OTP is ignored" >&2
    set -- ;;
  encryption|mandatory_encryption)
    : "${BLOB_OTP:?set BLOB_OTP (secret of the cafs server), or ENCRYPTION=none for a LAN proxy}"
    set -- "$ENCRYPTION" "$BLOB_OTP" ;;
  *) echo "ENCRYPTION must be none, encryption or mandatory_encryption" >&2; exit 1 ;;
esac
: "${CACHE_SOFT_LIMIT_SIZE:=102400}"
# shellcheck disable=SC2086
exec /usr/local/cvsnt/bin/cafs_proxy_server "$MASTER_URL" "$CACHE_FOLDER" $VALIDATE_BLOBS $UPDATE_MTIME "$@" "$CACHE_SOFT_LIMIT_SIZE"

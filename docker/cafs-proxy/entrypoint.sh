#!/bin/bash
# cafs_proxy_server master_url cache_folder [validate_blobs_from_master] [update_mtime_on_access] [encryption|mandatory_encryption secret] [cache_soft_limit_size_mb]
#
# BLOB_OTP is the shared secret; SECRET is accepted as an alias.
set -eu
: "${MASTER_URL:?set MASTER_URL (host name of the cafs server; port is always 2403)}"
: "${CACHE_FOLDER:=/var/cache/cafs}"
: "${VALIDATE_BLOBS:=}"
: "${UPDATE_MTIME:=}"
: "${ENCRYPTION:=mandatory_encryption}"
: "${BLOB_OTP:=${SECRET:-}}"
: "${BLOB_OTP:?set BLOB_OTP (secret of the cafs server)}"
case "$ENCRYPTION" in encryption|mandatory_encryption) ;; *) echo "ENCRYPTION must be encryption or mandatory_encryption" >&2; exit 1 ;; esac
: "${CACHE_SOFT_LIMIT_SIZE:=102400}"
# shellcheck disable=SC2086
exec /usr/local/cvsnt/bin/cafs_proxy_server "$MASTER_URL" "$CACHE_FOLDER" $VALIDATE_BLOBS $UPDATE_MTIME "$ENCRYPTION" "$BLOB_OTP" "$CACHE_SOFT_LIMIT_SIZE"

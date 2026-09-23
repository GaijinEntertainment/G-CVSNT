#!/bin/bash
# cafs_server dir_for_roots allow_trust(on|off) [norepack] [encryption|mandatory_encryption secret] [port] [max_pending_connections]
#
# SECRET is the shared secret; BLOB_OTP is accepted as an alias.
set -eu
: "${DIR_FOR_ROOTS:=/cvs}"
: "${ALLOW_TRUST:=on}"
: "${NOREPACK:=}"
: "${ENCRYPTION:=mandatory_encryption}"
: "${SECRET:=${BLOB_OTP:-}}"
: "${SECRET:?set SECRET (shared with BlobOTP in the cvsnt PServer config, 16 chars minimum)}"
case "$ENCRYPTION" in encryption|mandatory_encryption) ;; *) echo "ENCRYPTION must be encryption or mandatory_encryption" >&2; exit 1 ;; esac
: "${PORT:=2403}"
: "${MAX_PEND_CONN:=1024}"
# shellcheck disable=SC2086
exec /usr/local/cvsnt/bin/cafs_server "$DIR_FOR_ROOTS" "$ALLOW_TRUST" $NOREPACK "$ENCRYPTION" "$SECRET" "$PORT" "$MAX_PEND_CONN"

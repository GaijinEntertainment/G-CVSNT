#!/usr/bin/env bash
# Tear the contour down and drop the repository volume, so the next run
# starts from a fresh fixture. Dumps logs first when asked (any argument).
set -u
cd "$(dirname "$0")"
# up.sh exports these; when it never ran, compose still needs them to parse
export CONTOUR_CONF="${CONTOUR_CONF:-/nonexistent}" CAFS_SECRET="${CAFS_SECRET:-unset}"
if [ $# -gt 0 ] && [ -n "$(docker compose ps -q 2>/dev/null)" ]; then
  docker compose logs || true
  docker compose exec -T authserver cat /var/log/xinetd/xinetd.log || true
fi
docker compose down -v --remove-orphans

#!/usr/bin/env bash
set -u
cd "$(dirname "$0")"
export CONTOUR_CONF="${CONTOUR_CONF:-/nonexistent}" CAFS_SECRET="${CAFS_SECRET:-unset}"
if [ $# -gt 0 ] && [ -n "$(docker compose ps -q 2>/dev/null)" ]; then
  docker compose logs || true
  docker compose exec -T authserver cat /var/log/xinetd/xinetd.log || true
fi
docker compose down -v --remove-orphans

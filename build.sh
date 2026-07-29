#!/usr/bin/env bash
set -o pipefail
cd "$(dirname "$0")"
mkdir -p build_logs
stamp="$(date +%Y%m%d_%H%M%S)"
{
  echo "Pstro Laucher NDS KVM build log"
  echo "Time: $(date '+%Y-%m-%d %H:%M:%S')"
  echo "Project: $(pwd)"
  echo "ROM code: J2DS"
  echo "Runtime: FAT JAR launcher, no audio"
  echo "============================================================"
  make clean && make
} 2>&1 | tee last_build.log
status=${PIPESTATUS[0]}
cp -f last_build.log "build_logs/build_${stamp}.log"
exit "$status"

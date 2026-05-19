#!/usr/bin/env bash
# Verify that the resolved act job list matches tests-matrix.gold.
# Usage: bash .github/workflows/check-matrix.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GOLD="$SCRIPT_DIR/tests-matrix.gold"
ACT="${ACT:-act}"

got=$(
  "$ACT" -n -W "$SCRIPT_DIR/tests.yaml" 2>&1 \
    | grep -E 'DRYRUN.*Job succeeded' \
    | sed 's/\*DRYRUN\* \[//; s/\/Test\/test.*$//' \
    | sed 's/-[0-9]*$//' \
    | sort
)

if diff <(echo "$got") "$GOLD"; then
  echo "OK: matrix matches gold"
else
  echo "FAIL: matrix differs from $GOLD"
  exit 1
fi

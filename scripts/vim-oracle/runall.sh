#!/bin/bash
# Run every oracle suite and save the full results for mkheader.py.
# Usage: flock /tmp/lightpad-heavy.lock nice -n 19 bash runall.sh [suite ...]
cd "$(dirname "$0")"
suites=("$@")
if [ ${#suites[@]} -eq 0 ]; then
  suites=(audit gn misc more cmds grid)
fi
for s in "${suites[@]}"; do
  echo "== $s"
  timeout 1500 python3 harness.py "$s.json" "all_$s.json" | tail -1
  cp fails.json "fails_$s.json"
done

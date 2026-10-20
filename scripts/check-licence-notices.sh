#!/usr/bin/env bash
# scan-engine-android: fails when an Engine source file lacks the SANE
# exception.
# Copyright (C) 2026 Petar Popov
# GPL-2.0-or-later with the SANE exception; see README.md and COPYING.
#
# Usage: check-licence-notices.sh [REPO_DIR]
#
# Every .c, .h, .in, .kt, .java or .aidl file outside third_party/ runs in
# the Engine process or builds it, so it carries the full GPL notice with the
# SANE exception. That includes new files not yet added to git; git-ignored
# build output is skipped. third_party/ keeps upstream's notices and is
# covered by check-sane-exception.sh.
set -euo pipefail

repo=${1:-$(cd "$(dirname "$0")/.." && pwd)}

missing=0
checked=0
while IFS= read -r -d '' file; do
  checked=$((checked + 1))
  if ! grep -q "As a special exception" "$repo/$file"; then
    echo "MISSING the SANE exception: $file" >&2
    missing=1
  fi
done < <(git -C "$repo" ls-files -z --cached --others --exclude-standard -- \
  '*.c' '*.h' '*.in' '*.kt' '*.java' '*.aidl' ':!:third_party/**')

if [[ $missing -ne 0 ]]; then
  echo "Add the GPL-2.0-or-later notice with the SANE exception (copy it from src/engine.h) to the files above." >&2
  exit 1
fi
echo "All $checked Engine source files carry the SANE exception."

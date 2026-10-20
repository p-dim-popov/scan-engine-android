#!/usr/bin/env bash
# scan-engine-android: replaces a vendored tree with an upstream release.
# Copyright (C) 2026 Petar Popov
# GPL-2.0-or-later with the SANE exception; see README.md and COPYING.
#
# Usage: import-upstream.sh NAME SOURCE_DIR
#
# Mirrors SOURCE_DIR (an unpacked release or a checkout of its tag) into
# third_party/NAME and stages it with git add -f. Upstream trees ship their
# own .gitignore files, and a plain git add silently skips what they match:
# that is how the prototype lost config.h and the pixma option tables. The
# script fails unless git tracks exactly the files the release has.
# Commit the result alone, then reapply the Android changes on top.
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 NAME SOURCE_DIR" >&2
  exit 2
fi
repo=$(cd "$(dirname "$0")/.." && pwd)
name=$1
source=$(realpath "$2")
target=$repo/third_party/$name

rsync -a --delete --exclude .git "$source/" "$target/"
git -C "$repo" add -f -A "third_party/$name"

expected=$(cd "$source" && find . -path ./.git -prune -o \( -type f -o -type l \) -print | wc -l)
tracked=$(git -C "$repo" ls-files "third_party/$name" | wc -l)
if [[ $expected -ne $tracked ]]; then
  echo "third_party/$name: the release has $expected files but git tracks $tracked" >&2
  exit 1
fi
echo "third_party/$name: all $tracked files staged."

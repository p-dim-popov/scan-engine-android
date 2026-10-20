#!/usr/bin/env bash
# scan-engine-android: fails when a compiled sane-backends file lacks the SANE
# exception.
# Copyright (C) 2026 Petar Popov
# GPL-2.0-or-later with the SANE exception; see README.md and COPYING.
#
# Usage: check-sane-exception.sh BUILD_DIR SANE_DIR EXEMPTIONS
#
# BUILD_DIR is a finished Ninja build. Every file under SANE_DIR/backend or
# SANE_DIR/sanei that any object was compiled from, the headers and #included
# .c files too, must contain the exception text, unless EXEMPTIONS names it
# (one path relative to SANE_DIR per line, then the reason; # starts a comment).
# An exempt file must not mention the GNU GPL, and every exemption must name
# a compiled file, so a stale entry cannot linger.
# One exception-free file in the Engine process would put the whole process
# under the plain GPL, so this reruns on every upstream rebase.
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 BUILD_DIR SANE_DIR EXEMPTIONS" >&2
  exit 2
fi
build=$(realpath "$1")
sane=$(realpath "$2")
exemptions=$3
ninja=${NINJA:-ninja}
command -v "$ninja" >/dev/null || ninja=$(sed -n 's/^CMAKE_MAKE_PROGRAM:[A-Z]*=//p' "$build/CMakeCache.txt")

# Bring the dependency log up to date, then list every input of every object.
"$ninja" -C "$build" >/dev/null
mapfile -t compiled < <(
  "$ninja" -C "$build" -t deps \
    | sed -n 's/^    //p' \
    | while read -r path; do
        [[ $path = /* ]] || path=$build/$path
        path=$(realpath -m "$path")
        case $path in
          "$sane"/backend/* | "$sane"/sanei/*) echo "$path" ;;
        esac
      done \
    | sort -u
)
if [[ ${#compiled[@]} -eq 0 ]]; then
  echo "no compiled backend/ or sanei/ file found in $build: is it a Ninja build of the Engine?" >&2
  exit 1
fi

mapfile -t exempt < <(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' "$exemptions" | awk '{print $1}')

missing=0
for file in "${compiled[@]}"; do
  relative=${file#"$sane"/}
  if grep -q "As a special exception" "$file"; then
    continue
  fi
  if printf '%s\n' "${exempt[@]}" | grep -qxF "$relative"; then
    if grep -q "GNU General Public License" "$file"; then
      echo "EXEMPT BUT UNDER THE GNU GPL: $relative" >&2
      missing=1
    else
      echo "exempt: $relative"
    fi
    continue
  fi
  echo "MISSING the SANE exception: $relative" >&2
  missing=1
done

for relative in "${exempt[@]}"; do
  if ! printf '%s\n' "${compiled[@]}" | grep -qxF "$sane/$relative"; then
    echo "STALE exemption, not compiled: $relative" >&2
    missing=1
  fi
done

if [[ $missing -ne 0 ]]; then
  echo "Fix the files above: drop a backend whose files lack the SANE exception; exempt a file in $exemptions only when it is compiled and under a non-GPL permissive licence." >&2
  exit 1
fi
echo "All ${#compiled[@]} compiled backend/ and sanei/ files carry the SANE exception or a valid exemption."

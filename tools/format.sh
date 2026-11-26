#!/usr/bin/env bash
# Format all C++ sources with the pinned clang-format.
#
#   tools/format.sh           # rewrite files in place
#   tools/format.sh --check   # only report; exit 1 if anything is unformatted (used in CI)
#
# Requires uv (https://docs.astral.sh/uv/): `brew install uv`.
set -euo pipefail

CLANG_FORMAT_VERSION="23.1.5"
cd "$(git rev-parse --show-toplevel)"

files=()
while IFS= read -r f; do files+=("$f"); done < <(git ls-files --cached --others --exclude-standard '*.cpp' '*.h' '*.hpp' '*.mm')

if [[ ${#files[@]} -eq 0 ]]; then
    echo "No C++ files found."
    exit 0
fi

run() { uvx -q --from "clang-format==${CLANG_FORMAT_VERSION}" clang-format "$@"; }

if [[ "${1:-}" == "--check" ]]; then
    run --dry-run --Werror "${files[@]}"
    echo "Formatting OK (${#files[@]} files)."
else
    run -i "${files[@]}"
    echo "Formatted ${#files[@]} files."
fi

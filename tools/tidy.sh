#!/usr/bin/env bash
# Run clang-tidy (pinned version) on Melisma's own sources.
#
#   tools/tidy.sh [build-dir]    # default build dir: build/debug (cmake --preset debug)
#   TIDY_STRICT=1 tools/tidy.sh  # treat every warning as an error (used in CI)
#
# Needs compile_commands.json, which CMake writes on configure.
# Requires uv (https://docs.astral.sh/uv/): `brew install uv`.
set -euo pipefail

CLANG_TIDY_VERSION="23.1.4"
cd "$(git rev-parse --show-toplevel)"
BUILD_DIR="${1:-build/debug}"

if [[ ! -f "${BUILD_DIR}/compile_commands.json" ]]; then
    echo "No ${BUILD_DIR}/compile_commands.json. Run: cmake --preset debug" >&2
    exit 2
fi

extra=()
if [[ "$(uname)" == "Darwin" ]]; then
    # The PyPI clang-tidy does not know where Apple's SDK lives.
    extra+=(--extra-arg="-isysroot$(xcrun --show-sdk-path)")
fi

if [[ "${TIDY_STRICT:-0}" == "1" ]]; then
    extra+=(--warnings-as-errors='*')
fi

files=()
while IFS= read -r f; do files+=("$f"); done < <(git ls-files --cached --others --exclude-standard '*.cpp')

uvx -q --from "clang-tidy==${CLANG_TIDY_VERSION}" clang-tidy \
    -p "${BUILD_DIR}" --quiet "${extra[@]}" "${files[@]}"

#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly repo_root
readonly image="fritzmonitor-markdown-tools:local"
readonly markdown_files=(README.md FRITZMONITOR.md QUICK-SETUP.md COPYRIGHT.md)

usage() {
  echo "Usage: $0 check|format" >&2
}

mode=${1:-}
if [[ $# -ne 1 || ( "$mode" != check && "$mode" != format ) ]]; then
  usage
  exit 2
fi

docker build --file "$repo_root/Dockerfile.markdown" --tag "$image" "$repo_root"

if [[ "$mode" == check ]]; then
  docker run --rm \
    --volume "$repo_root:/workspace:ro" \
    "$image" \
    sh -c 'prettier --check "$@" && markdownlint-cli2 "$@"' \
    sh "${markdown_files[@]}"
else
  docker run --rm \
    --user "$(id -u):$(id -g)" \
    --volume "$repo_root:/workspace" \
    "$image" \
    sh -c 'prettier --write "$@" && markdownlint-cli2 "$@"' \
    sh "${markdown_files[@]}"
fi

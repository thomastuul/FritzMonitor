#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
artifact_dir="$project_root/build/container-release"
image_name=fritzmonitor-build
docker_config=$(mktemp -d)

mkdir -p "$artifact_dir"
cleanup() {
  rm -rf "$docker_config"
}
trap cleanup EXIT

DOCKER_CONFIG="$docker_config" docker build --target artifact --tag "$image_name" "$project_root"

container_id=$(DOCKER_CONFIG="$docker_config" docker create "$image_name")
trap 'DOCKER_CONFIG="$docker_config" docker rm "$container_id" >/dev/null; rm -rf "$docker_config"' EXIT

DOCKER_CONFIG="$docker_config" docker cp "$container_id:/fritzmonitor" "$artifact_dir/fritzmonitor"
DOCKER_CONFIG="$docker_config" docker cp "$container_id:/fritzmonitor.service" "$artifact_dir/fritzmonitor.service"
DOCKER_CONFIG="$docker_config" docker cp "$container_id:/fritzmonitor-phone-green.svg" "$artifact_dir/fritzmonitor-phone-green.svg"
DOCKER_CONFIG="$docker_config" docker cp "$container_id:/fritzmonitor-phone-red.svg" "$artifact_dir/fritzmonitor-phone-red.svg"
DOCKER_CONFIG="$docker_config" docker cp "$container_id:/fritzmonitor-phone-yellow.svg" "$artifact_dir/fritzmonitor-phone-yellow.svg"
chmod 0755 "$artifact_dir/fritzmonitor"

printf 'Artifacts written to %s\n' "$artifact_dir"

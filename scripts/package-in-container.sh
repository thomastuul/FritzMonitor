#!/usr/bin/env bash
set -euo pipefail

format=${1:-}
case "$format" in
  deb)
    image=debian:trixie
    packages='build-essential cmake pkg-config file libglib2.0-dev libgtk-3-dev libnotify-dev libayatana-appindicator3-dev libcurl4-openssl-dev libxml2-dev dpkg-dev'
    target=package-deb
    ;;
  rpm)
    image=fedora:latest
    packages='gcc-c++ cmake pkgconf-pkg-config glib2-devel gtk3-devel libnotify-devel libayatana-appindicator-gtk3-devel libcurl-devel libxml2-devel rpm-build'
    target=package-rpm
    ;;
  *)
    printf 'Usage: %s {deb|rpm}\n' "$0" >&2
    exit 2
    ;;
esac

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

docker run --rm \
  --volume "$project_root:/src" \
  --workdir /src \
  "$image" \
  bash -euxo pipefail -c "
    if command -v apt-get >/dev/null; then
      export DEBIAN_FRONTEND=noninteractive
      apt-get update
      apt-get install -y --no-install-recommends $packages
      rm -rf /var/lib/apt/lists/*
    else
      dnf install -y $packages
    fi
    rm -rf build/package-$format
    cmake -S . -B build/package-$format -DCMAKE_BUILD_TYPE=Release \\
      -DBUILD_TESTING=ON \\
      -DCMAKE_INSTALL_PREFIX=/usr \\
      -DFRITZMONITOR_SERVICE_EXECUTABLE=/usr/bin/fritzmonitor
    cmake --build build/package-$format --target $target --parallel
  "

printf 'Package artifacts are in %s/build/package-%s\n' "$project_root" "$format"

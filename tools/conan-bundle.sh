#!/usr/bin/env bash
# Builds the cppi Conan package for one configuration and saves it, with the
# recipe, its sources and the binaries of its dependencies, as a bundle that
# `conan cache restore` loads into any Conan cache without a remote.
#
#   tools/conan-bundle.sh PROFILE BUILD_TYPE OUTPUT.tgz [extra conan create args...]
#
#   tools/conan-bundle.sh conan/profiles/ci Release dist/cppi-linux-x86_64-release.tgz
#
# The package is built as games consume it: static, without cppi-run, and
# without cppi's own tests (test_package still checks that it links). Only
# host packages go in the bundle; tools (emsdk, cmake) stay out.
#
# Author: kevin-mcm <kevincardenasmiranda9@gmail.com>
# Date:   2026-10-09
set -euo pipefail

if [[ $# -lt 3 ]]; then
  sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
  exit 2
fi

profile=$1
build_type=$2
output=$3
shift 3

root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

conan create "$root" --build=missing \
  -pr:h "$profile" -pr:b default -s build_type="$build_type" \
  -o "cppi/*:shared=False" -o "cppi/*:with_tools=False" \
  -c tools.build:skip_test=True \
  "$@" --format=json >"$work/graph.json"

conan list --graph="$work/graph.json" \
  --graph-context=host-only --format=json >"$work/packages.json"

mkdir -p "$(dirname "$output")"
conan cache save --list="$work/packages.json" --file="$output"
echo "Saved $output"

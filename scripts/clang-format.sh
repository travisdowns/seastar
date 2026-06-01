#!/usr/bin/env bash
set -euo pipefail

# Run clang-format 22 for Seastar: prefer local tools, fall back to Docker,
# and when no args are given, format all .cc/.hh files under src and include.

# Set CLANG_FORMAT_FORCE_DOCKER=1 to skip local clang-format detection
# and always run via Docker.
force_docker="${CLANG_FORMAT_FORCE_DOCKER:-0}"

clang_args=("$@")
if [[ $# -eq 0 ]]; then
	mapfile -t clang_args < <(
		find src include -type f \( -name "*.cc" -o -name "*.hh" \) 2>/dev/null | sort
	)
	echo "Formatting ${#clang_args[@]} files from src and include" >&2
	if [[ ${#clang_args[@]} -eq 0 ]]; then
		exit 0
	fi
	clang_args=(-i "${clang_args[@]}")
fi

if [[ "$force_docker" != "1" ]]; then
	# Prefer a native clang-format 22 when available.
	if command -v clang-format-22 >/dev/null 2>&1; then
		exec clang-format-22 "${clang_args[@]}"
	fi

	if command -v clang-format >/dev/null 2>&1; then
		cf_version="$(clang-format --version 2>/dev/null || true)"
		if [[ "$cf_version" =~ (^|[[:space:]])version[[:space:]]+22([.[:space:]]|$) ]]; then
			exec clang-format "${clang_args[@]}"
		fi
	fi
fi

# Fall back to Docker image with clang-format-22 when docker is available.
if command -v docker >/dev/null 2>&1; then
	script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
	docker_dir="$script_dir/clang-format-files"
	image_name="seastar/clang-format-22:local"

	if [[ ! -d "$docker_dir" ]]; then
		echo "Missing docker support directory: $docker_dir" >&2
		exit 1
	fi

	if ! docker image inspect "$image_name" >/dev/null 2>&1; then
		echo "Building Docker image $image_name (this may take a while)..." >&2
		docker build -t "$image_name" "$docker_dir"
	fi

	exec docker run --rm -i \
		-u "$(id -u):$(id -g)" \
		-v "$PWD:/work" \
		-w /work \
		"$image_name" \
		"${clang_args[@]}"
fi

echo "Could not find clang-format 22 locally and docker is not available." >&2
echo "Install clang-format-22, or install docker." >&2
exit 1

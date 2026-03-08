#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: tools/build_diligent.sh [--config <name>] [--build-dir <path>] [--package-dir <path>] [--clean]

Options:
  --config       Build configuration: Debug, Release, RelWithDebInfo, Profile, Retail
  --build-dir    Override the build directory
  --package-dir  Override the generated HiveDiligent package directory
  --clean        Remove the build directory before configuring
EOF
}

config="Debug"
build_dir=""
package_dir=""
clean=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --config)
            config="${2:-}"
            shift 2
            ;;
        --build-dir)
            build_dir="${2:-}"
            shift 2
            ;;
        --package-dir)
            package_dir="${2:-}"
            shift 2
            ;;
        --clean)
            clean=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

case "$config" in
    Debug|Release|RelWithDebInfo|Profile|Retail)
        ;;
    *)
        echo "Unsupported config: $config" >&2
        exit 1
        ;;
esac

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root_dir="$(cd "${script_dir}/.." && pwd)"
source_dir="${root_dir}/tools/diligent"
config_lower="$(printf '%s' "$config" | tr '[:upper:]' '[:lower:]')"

if [[ -z "$build_dir" ]]; then
    build_dir="${root_dir}/out/build/diligent-${config_lower}"
fi

if [[ -z "$package_dir" ]]; then
    package_dir="${build_dir}/package"
fi

if [[ "$clean" -eq 1 && -d "$build_dir" ]]; then
    rm -rf "$build_dir"
fi

mkdir -p "$build_dir" "$package_dir"

generator="Ninja"
if ! command -v ninja >/dev/null 2>&1; then
    generator="Unix Makefiles"
fi

cmake_args=(
    -S "$source_dir"
    -B "$build_dir"
    -G "$generator"
    "-DCMAKE_BUILD_TYPE=${config}"
    "-DHIVE_ROOT_DIR=${root_dir}"
    "-DHIVE_DILIGENT_EXPORT_DIR=${package_dir}"
)

compiler_launcher=""
if command -v sccache >/dev/null 2>&1; then
    compiler_launcher="$(command -v sccache)"
elif command -v ccache >/dev/null 2>&1; then
    compiler_launcher="$(command -v ccache)"
fi

if [[ -n "$compiler_launcher" ]]; then
    cmake_args+=(
        "-DCMAKE_C_COMPILER_LAUNCHER=${compiler_launcher}"
        "-DCMAKE_CXX_COMPILER_LAUNCHER=${compiler_launcher}"
    )
fi

cmake "${cmake_args[@]}"
cmake --build "$build_dir" --config "$config" --target HiveDiligentPackage

echo "HiveDiligent package ready:"
echo "  Config:  $config"
echo "  Build:   $build_dir"
echo "  Package: $package_dir"
echo "Use -DHIVE_DILIGENT_PROVIDER=PREBUILT to require this package, or leave AUTO for fallback."

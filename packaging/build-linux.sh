#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-${repo}/out/build/linux-package}"
install_dir="${INSTALL_DIR:-${repo}/out/install/linux-package}"
dist_dir="${DIST_DIR:-${repo}/dist}"
cmake_version="${CMAKE_VERSION:-3.28.6}"
package_arch="${PACKAGE_ARCH:-$(uname -m)}"

if [[ "$(id -u)" -eq 0 ]]; then
    if command -v yum >/dev/null; then
        yum install -y epel-release
        yum install -y make gcc gcc-c++ ca-certificates curl file patchelf
    elif command -v apt-get >/dev/null; then
        apt-get update
        DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
            build-essential ca-certificates curl file patchelf
    else
        echo "Unsupported package manager; install GCC, curl, file and patchelf" >&2
        exit 1
    fi
fi

if ! command -v cmake >/dev/null || ! cmake --version | head -1 | grep -Eq 'version 3\.(2[1-9]|[3-9][0-9])'; then
    case "$(uname -m)" in
        x86_64) cmake_arch=x86_64 ;;
        aarch64) cmake_arch=aarch64 ;;
        *) echo "Unsupported architecture: $(uname -m)" >&2; exit 1 ;;
    esac
    curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${cmake_version}/cmake-${cmake_version}-linux-${cmake_arch}.tar.gz" \
        | tar -xz --strip-components=1 -C /usr/local
fi

cmake_args=()
if [[ -n "${ZIG_TARGET:-}" ]]; then
    cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=${repo}/packaging/zig-toolchain.cmake")
fi

rm -rf "${build_dir}" "${install_dir}"
cmake -S "${repo}" -B "${build_dir}" -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${install_dir}" \
    "${cmake_args[@]}"
cmake --build "${build_dir}" --parallel "$(nproc)"
cmake --install "${build_dir}"

PACKAGE_ARCH="${package_arch}" CROSS_COMPILE="${ZIG_TARGET:-}" \
    bash "${repo}/packaging/package-linux.sh" "${install_dir}" "${dist_dir}"

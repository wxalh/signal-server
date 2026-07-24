#!/usr/bin/env bash
set -euo pipefail

prefix="${1:-/opt/signal-server}"
output_dir="${2:-/dist}"
package_name="signal-server-linux-${PACKAGE_ARCH:-$(uname -m)}"
stage="${output_dir}/${package_name}"
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

rm -rf "${stage}"
mkdir -p "${stage}/lib"
cp -a "${prefix}/signal_server" "${stage}/"
find "${prefix}" -maxdepth 1 -type f -name '*.ini' -exec cp -a {} "${stage}/" \;
mkdir -p "${stage}/licenses"
cp -a "${repo}/LICENSE" "${stage}/licenses/project-LICENSE"
cp -a "${repo}/third_party/spdlog/LICENSE" "${stage}/licenses/spdlog-LICENSE"
cp -a "${repo}/third_party/asio/asio/LICENSE_1_0.txt" "${stage}/licenses/asio-LICENSE"
cp -a "${repo}/third_party/websocketpp/COPYING" "${stage}/licenses/websocketpp-LICENSE"
cp -a "${repo}/third_party/json/LICENSE.MIT" "${stage}/licenses/nlohmann-json-LICENSE"

# Bundle every recursively resolved shared library except glibc and its loader.
# Those two define the host ABI and must come from the target system.
declare -A visited
queue=("${stage}/signal_server")
while [[ -z "${CROSS_COMPILE:-}" ]] && ((${#queue[@]})); do
    elf="${queue[0]}"
    queue=("${queue[@]:1}")
    while read -r lib; do
        [[ -n "${lib}" && -f "${lib}" ]] || continue
        base="$(basename "${lib}")"
        case "${base}" in
            linux-vdso.so.*|ld-linux*.so.*|libc.so.*|libpthread.so.*|libdl.so.*|librt.so.*|libm.so.*|libresolv.so.*)
                continue ;;
        esac
        [[ -z "${visited[${lib}]:-}" ]] || continue
        visited["${lib}"]=1
        cp -aL "${lib}" "${stage}/lib/${base}"
        queue+=("${stage}/lib/${base}")
    done < <(ldd "${elf}" | awk '/=> \// { print $3 } /^\// { print $1 }')
done

if readelf -d "${stage}/signal_server" 2>/dev/null | grep -q '(NEEDED)'; then
    patchelf --set-rpath '$ORIGIN/lib' "${stage}/signal_server"
    find "${stage}/lib" -type f -exec patchelf --set-rpath '$ORIGIN' {} \; 2>/dev/null || true
fi

if [[ -n "${CROSS_COMPILE:-}" ]]; then
    unexpected_dependencies="$(
        readelf -d "${stage}/signal_server" 2>/dev/null \
            | sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p' \
            | grep -Ev '^(libc|libpthread|libdl|libm|librt|libresolv)\.so\.|^ld-linux[^/]*\.so\.' \
            || true
    )"
    if [[ -n "${unexpected_dependencies}" ]]; then
        echo "ERROR: cross-compiled release has unsupported dynamic dependencies:" >&2
        printf '%s\n' "${unexpected_dependencies}" >&2
        exit 1
    fi
fi

if [[ -n "${CROSS_COMPILE:-}" ]]; then
    printf 'C++ runtime and third-party dependencies are statically linked.\nSystem-provided ABI libraries: glibc and the Linux dynamic loader.\n' > "${stage}/DEPENDENCIES.txt"
else
    {
        printf 'Bundled shared libraries:\n'
        find "${stage}/lib" -maxdepth 1 -type f -printf '%f\n' | sort
        printf '\nSystem-provided ABI libraries: glibc and the Linux dynamic loader.\n'
    } > "${stage}/DEPENDENCIES.txt"
fi

max_glibc=217
while IFS= read -r elf; do
    while read -r version; do
        numeric="${version#GLIBC_}"
        major="${numeric%%.*}"
        minor="${numeric#*.}"
        (( major * 100 + minor <= max_glibc )) || {
            echo "ERROR: ${elf} requires ${version}; the supported baseline is GLIBC_2.17" >&2
            exit 1
        }
    done < <(readelf --version-info "${elf}" 2>/dev/null | grep -o 'GLIBC_[0-9]\+\.[0-9]\+' | sort -Vu)
done < <(find "${stage}" -type f -exec sh -c 'file "$1" | grep -q ELF && printf "%s\n" "$1"' _ {} \;)

tar -C "${output_dir}" -czf "${output_dir}/${package_name}.tar.gz" "${package_name}"
sha256sum "${output_dir}/${package_name}.tar.gz" > "${output_dir}/${package_name}.tar.gz.sha256"
echo "Created ${output_dir}/${package_name}.tar.gz"

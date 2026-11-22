#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
case "$(uname -m)" in
    x86_64) arch=amd64; machine='Advanced Micro Devices X86-64' ;;
    aarch64) arch=arm64; machine='AArch64' ;;
    *) echo 'fnOS packages require an x86_64 or aarch64 Linux build host' >&2; exit 1 ;;
esac

dist="${DIST_DIR:-${repo}/dist}"
work="$(mktemp -d "${TMPDIR:-/tmp}/signal-fnos-build.XXXXXXXX")"
trap 'rm -rf -- "$work"' EXIT
payload="$work/payload"
version="$(sed -nE 's/^project\(signal_server VERSION ([0-9]+\.[0-9]+\.[0-9]+) .*/\1/p' "$repo/CMakeLists.txt")"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]

cmake -S "$repo" -B "$work/signal" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DSIGNAL_SERVER_PORTABLE_GLIBC=ON
cmake --build "$work/signal" --parallel "${BUILD_JOBS:-2}"
cmake --install "$work/signal" --prefix "$work/signal-install"
install -D -m 0755 "$work/signal-install/signal_server" "$payload/bin/signal_server"
cp -a "$work/signal-install/licenses" "$payload/licenses"

export CGO_ENABLED=0
(
    cd "$repo/packaging/fnos/runtime"
    go test ./...
    # Container builds may not trust the bind-mounted checkout's Git metadata.
    go build -buildvcs=false -trimpath -ldflags='-s -w' -o "$payload/bin/fnos-manager" .
)
GOROOT_DIR="$(go env GOROOT)"
install -m 0644 "$GOROOT_DIR/LICENSE" "$payload/licenses/go-LICENSE.txt"
install -m 0644 "$GOROOT_DIR/PATENTS" "$payload/licenses/go-PATENTS.txt"
bash "$repo/packaging/fnos/build-coturn.sh" "$payload"

for elf in "$payload/bin/"*; do
    readelf -h "$elf" | grep -F "Machine:" | grep -Fq "$machine"
    while read -r symbol; do
        numeric="${symbol#GLIBC_}"
        major="${numeric%%.*}"
        rest="${numeric#*.}"
        minor="${rest%%.*}"
        if (( major > 2 || (major == 2 && minor > 36) )); then
            printf '%s requires %s (fnOS baseline: glibc 2.36)\n' "$elf" "$symbol" >&2
            exit 1
        fi
    done < <(readelf --version-info "$elf" | grep -oE 'GLIBC_[0-9]+\.[0-9]+(\.[0-9]+)?' | sort -u)
done

bash "$repo/packaging/fnos/package.sh" "$arch" "$version" "$payload" "$dist"

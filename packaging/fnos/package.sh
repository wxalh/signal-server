#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" != 4 ]]; then
    echo 'Usage: package.sh <amd64|arm64> <version> <payload-directory> <output-directory>' >&2
    exit 2
fi

arch="$1"
version="$2"
case "$arch" in
    amd64) platform=x86 ;;
    arm64) platform=arm ;;
    *) printf 'Unsupported fnOS architecture: %s\n' "$arch" >&2; exit 2 ;;
esac
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[A-Za-z0-9]+([.-][A-Za-z0-9]+)*)?$ ]]; then
    printf 'Invalid fnOS version: %s\n' "$version" >&2
    exit 2
fi

template="$(cd "$(dirname "${BASH_SOURCE[0]}")/template" && pwd)"
payload="$(cd "$3" && pwd)"
mkdir -p -- "$4"
output="$(cd "$4" && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/signal-fnos-package.XXXXXXXX")"
trap 'rm -rf -- "$work"' EXIT
stage="$work/stage"
mkdir -p "$stage/wizard" "$stage/app/ui/images" "$stage/cmd" "$stage/config"

for binary in fnos-manager signal_server turnserver; do
    if [[ ! -x "$payload/bin/$binary" ]]; then
        printf 'Missing package executable: %s\n' "$payload/bin/$binary" >&2
        exit 1
    fi
done
cp -a "$payload/." "$stage/app/"
sed -e "s/@VERSION@/$version/g" -e "s/@PLATFORM@/$platform/g" \
    "$template/manifest" > "$stage/manifest"
install -m 0644 "$template/config/privilege" "$stage/config/privilege"
install -m 0644 "$template/config/resource" "$stage/config/resource"
install -m 0644 "$template/app/ui/config" "$stage/app/ui/config"
install -m 0755 "$template/cmd/main" "$stage/cmd/main"
# fnpack requires all eight lifecycle hooks even though this app initializes
# and reads its state in cmd/main.
for operation in install upgrade uninstall config; do
    for phase in init callback; do
        hook="$stage/cmd/${operation}_${phase}"
        printf '#!/bin/sh\nexit 0\n' > "$hook"
        chmod 0755 "$hook"
    done
done
install -m 0644 "$template/ICON.PNG" "$stage/app/ui/images/icon_64.png"
install -m 0644 "$template/ICON_256.PNG" "$stage/app/ui/images/icon_256.png"
install -m 0644 "$template/ICON.PNG" "$stage/ICON.PNG"
install -m 0644 "$template/ICON_256.PNG" "$stage/ICON_256.PNG"

if [[ -n "${FN_PACK:-}" ]]; then
    fnpack="$(command -v -- "$FN_PACK")"
    fnpack="$(realpath -- "$fnpack")"
else
    case "$(uname -m)" in
        x86_64)
            tool_arch=amd64
            digest=1464d6aa9b995ebecb263c5ec60f0f4a7cbf45e0ce887836755fd65abd647d1e
            ;;
        aarch64)
            tool_arch=arm
            digest=213c05a3df07ff881110d9f227926cd2d19601fc4f621815e673bd9ec0ff9373
            ;;
        *) echo 'Unsupported host architecture; set FN_PACK to a local fnpack binary.' >&2; exit 1 ;;
    esac
    fnpack="$work/fnpack"
    curl --fail --silent --show-error --location --retry 3 \
        "https://static2.fnnas.com/fnpack/fnpack-1.3.3-linux-$tool_arch" -o "$fnpack"
    printf '%s  %s\n' "$digest" "$fnpack" | sha256sum --check --status
    chmod 0755 "$fnpack"
fi

build_flags=()
if [[ -n "${FN_PACK_KEY:-}" || -n "${FN_PACK_CERT:-}" ]]; then
    if [[ ! -f "${FN_PACK_KEY:-}" || ! -f "${FN_PACK_CERT:-}" ]]; then
        echo 'Set both FN_PACK_KEY and FN_PACK_CERT to the developer signing files.' >&2
        exit 2
    fi
    build_flags+=(--key "$(realpath -- "$FN_PACK_KEY")" --cert "$(realpath -- "$FN_PACK_CERT")")
elif [[ "$("$fnpack" build --help)" == *--nosign* ]]; then
    build_flags+=(--nosign)
fi
(cd "$stage" && "$fnpack" build "${build_flags[@]}")
name="signal-server-fnos-$version-$arch.fpk"
if [[ ! -s "$stage/signal-server.fpk" ]]; then
    echo 'fnpack did not produce an application archive.' >&2
    exit 1
fi
install -m 0644 "$stage/signal-server.fpk" "$output/$name"
(cd "$output" && sha256sum "$name" > "$name.sha256")
printf 'Created %s\n' "$output/$name"

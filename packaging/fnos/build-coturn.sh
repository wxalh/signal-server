#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

if [[ $# != 1 || -z "$1" ]]; then
    echo "Usage: $0 PAYLOAD_DIR" >&2
    exit 2
fi

version=4.19.2
sha256=2c80e4dffd0eb410951ba469f5cf8bd9b4a0658afc72d816f5c519c0c988d75c
url="https://codeload.github.com/coturn/coturn/tar.gz/refs/tags/${version}"
stage="$(realpath -m -- "$1")"
build="$(mktemp -d)"
trap 'rm -rf -- "$build"' EXIT

cd "$build"
curl --fail --silent --show-error --location --retry 3 --connect-timeout 20 \
    --output coturn.tar.gz "$url"
printf '%s  %s\n' "$sha256" coturn.tar.gz | sha256sum --check -
tar -xzf coturn.tar.gz
cd "coturn-${version}"

# The bundled server uses credentials from its app config and needs no databases.
TURN_NO_SQLITE=1 TURN_NO_PQ=1 TURN_NO_MYSQL=1 TURN_NO_MONGO=1 TURN_NO_HIREDIS=1 \
    TURN_NO_PROMETHEUS=1 TURN_NO_SYSTEMD=1 TURN_NO_SCTP=1 \
    CFLAGS='-O2 -fstack-protector-strong -D_FORTIFY_SOURCE=2' \
    ./configure --disable-rpath
make -j"${BUILD_JOBS:-$(nproc)}" bin/turnserver \
    LDFLAGS='-Wl,-z,relro,-z,now -Wl,-Bstatic -levent_openssl -levent_pthreads -levent -lssl -lcrypto -Wl,-Bdynamic -pthread -ldl'

strip --strip-unneeded bin/turnserver
deps="$(readelf -d bin/turnserver | sed -n 's/.*(NEEDED).*\[\([^]]*\)\].*/\1/p')"
while IFS= read -r dep; do
    case "$dep" in
        libc.so.6|libm.so.6|libpthread.so.0|libdl.so.2|librt.so.1|ld-linux-x86-64.so.2|ld-linux-aarch64.so.1) ;;
        *) echo "Unexpected shared dependency in turnserver: $dep" >&2; exit 1 ;;
    esac
done <<< "$deps"
glibc="$(readelf --version-info bin/turnserver | grep -oE 'GLIBC_[0-9]+(\.[0-9]+)+' | sort -Vu | tail -1)"
if [[ "$(printf '%s\n' "$glibc" GLIBC_2.36 | sort -V | tail -1)" != GLIBC_2.36 ]]; then
    echo "turnserver requires ${glibc}, newer than the fnOS glibc 2.36 baseline" >&2
    exit 1
fi
if [[ "$(bin/turnserver --version)" != "$version" ]]; then
    echo "Built turnserver did not report version $version" >&2
    exit 1
fi

install -d "$stage/bin" "$stage/licenses"
install -m 0755 bin/turnserver "$stage/bin/turnserver"
install -m 0644 LICENSE "$stage/licenses/coturn-LICENSE.txt"
install -m 0644 /usr/share/doc/libevent-dev/copyright "$stage/licenses/coturn-libevent-copyright.txt"
install -m 0644 /usr/share/doc/libssl-dev/copyright "$stage/licenses/coturn-OpenSSL-copyright.txt"
install -m 0644 /usr/share/common-licenses/Apache-2.0 "$stage/licenses/coturn-OpenSSL-Apache-2.0.txt"
{
    printf 'coturn %s\nSource: %s\nSource SHA-256: %s\n' "$version" "$url" "$sha256"
    dpkg-query -W -f='${Package} ${Version}\n' libevent-dev libssl-dev
    printf 'glibc requirement: %s\n' "$glibc"
} > "$stage/licenses/coturn-BUILD.txt"

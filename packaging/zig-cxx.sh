#!/usr/bin/env bash
set -e
exec "${ZIG:-zig}" c++ -target "${ZIG_TARGET:?ZIG_TARGET is required}" "$@"

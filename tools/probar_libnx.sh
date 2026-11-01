#!/usr/bin/env bash
# Compila un homebrew de guest/libnx/<nombre> con devkitPro, lo recompila con --traza y lo corre.
set -euo pipefail
cd "$(dirname "$0")/.."
n=${1:?uso: tools/probar_libnx.sh <nombre>}
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
make -s -C "guest/libnx/$n" >/dev/null
python3 recomp/nxrecomp.py "guest/libnx/$n/$n.nro" -o "build/${n}_rec" --traza >/dev/null
python3 tools/compilar.py "build/${n}_rec" -o "build/$n" -O1 >/dev/null
shift
timeout "${TIEMPO:-20}" "build/$n" "$@"

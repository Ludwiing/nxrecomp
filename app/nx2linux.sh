#!/bin/sh
# Lanzador de NX2Linux (lo usan la entrada del menú y las de cada juego).
cd "$(dirname "$(readlink -f "$0")")" && exec python3 -m nx2linux "$@"

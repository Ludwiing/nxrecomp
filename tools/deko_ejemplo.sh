#!/bin/sh
# Corre el ejemplo N (1-9) de deko_examples y captura el cuadro que sigue a CUADROS de haberlo elegido.
# Uso: tools/deko_ejemplo.sh N salida.ppm [CUADROS]
n=$1; out=$2; extra=${3:-40}
f=10; pulsar=""
i=2; while [ $i -le $n ]; do pulsar="$pulsar$f:down,"; f=$((f + 4)); i=$((i + 1)); done
pulsar="$pulsar$((f + 4)):a"
NX_SIN_VENTANA=1 NX_PULSAR="$pulsar" NX_CAPTURA="$out" NX_CAPTURA_CUADRO=$((f + 4 + extra)) \
    timeout ${TIEMPO:-60} "$(dirname "$0")/../build/libnx_deko_examples"

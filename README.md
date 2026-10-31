# nxrecomp

Prueba de concepto: convertir código de Nintendo Switch (ARM64) en ejecutables **nativos de Linux x86_64**, sin emulador.

La idea es la de [nxnative](https://github.com/seedds/nxnative), llevada a una PC con Linux. nxnative puede ejecutar el código del juego tal cual porque los chips de Apple son ARM64. Un Ryzen o un Intel no lo son, así que aquí el código se traduce **una sola vez, antes de jugar** (recompilación estática, como N64Recomp o XenonRecomp). Al jugar no hay intérprete ni JIT: corre un programa normal de Linux.

## Qué hace hoy

```
guest/*.c (ARM64) ──clang──▶ .elf ──mknro──▶ .nro ──nxrecomp──▶ _rec.c ──clang──▶ build/<programa> (x86_64)
```

1. `guest/` tiene programas de prueba en formato homebrew de Switch: cabecera NRO, MOD0 y llamadas `svc` al kernel de Horizon.
2. `tools/mknro.py` los empaqueta como `.nro`.
3. `recomp/nxrecomp.py` lee el NRO, descubre las funciones y traduce cada instrucción a C. El decodificador está en `recomp/a64.py` (enteros y memoria), `recomp/simd.py` (FP escalar y cargas SIMD) y `recomp/neon.py` (NEON y cripto).
4. `runtime/` carga la imagen en la misma dirección que en la Switch, arma la pila y el TLS, y responde las llamadas al kernel. También tiene la semántica exacta de ARM para punto flotante (`nxfp.h`), NEON (`nxsimd.h`) y cripto (`nxcrypto.h`).

```
make test
```

compila cada programa para la Switch, lo recompila y compara su salida con la del mismo código compilado directo para x86_64:
- `hola`: enteros. Pasa en `-O0` a `-Oz` (`make test OPT=-Os`).
- `neon`: punto flotante y bucles vectorizados con `-O3`. Usa FMUL/FADD, TBL, ST3, SQXTN2, SHRN2, UMLAL2, UZP2, REV64, CNT, ADDV, BSL, FCVTZS y más. Los resultados coinciden bit a bit.

## El conjunto de instrucciones: completo para la CPU de la Switch

La Switch usa un Cortex-A57, que es ARMv8.0-A con cripto y CRC32. El traductor cubre todo lo que ese procesador ejecuta:
- **Enteros**: aritmética con banderas, lógica, campos de bits, multiplicación y división con la semántica de ARM, CRC32/CRC32C.
- **Memoria**: todas las formas de carga y guardado, pares, exclusivas, adquirir/liberar y LD1-LD4/ST1-ST4.
- **Punto flotante escalar**: con NaN, ceros con signo, saturación y conversiones de media precisión idénticas a ARM.
- **NEON**: los ~350 casos vectoriales y escalares, incluidas las estimaciones FRECPE/FRSQRTE con las tablas del manual.
- **Cripto**: AES, SHA1, SHA256 y PMULL de 64 bits.

### Cómo se verifica

`tests/fuzz.py` genera instrucciones al azar y ejecuta cada una dos veces con el mismo estado inicial, aleatorio, con NaN, infinitos y valores límite: en [Unicorn](https://www.unicorn-engine.org/), que hace de CPU ARM64 de referencia, y en su traducción a C. Compara x0-x30, sp, NZCV, V0-V31 y la memoria.

```
python3 -m venv .venv && .venv/bin/pip install unicorn capstone
.venv/bin/python tests/fuzz.py --clase simd --n 30000     # int, mem, memv, fp, simd
.venv/bin/python tests/coverage.py --clase simd --n 300000 # instrucciones válidas que el traductor rechace
```

Hoy da 0 diferencias en todas las clases (cientos de miles de instrucciones con varias semillas) y 0 instrucciones válidas rechazadas. Solo se rechazan las codificaciones UNPREDICTABLE, que ningún compilador genera.

Unicorn solo se usa para estas pruebas. Los ejecutables no lo incluyen.

## Límites conocidos

- **Tablas de salto** de los `switch`: un `BR` a una etiqueta dentro de la función todavía no se resuelve. Por eso las pruebas usan `-fno-jump-tables`.
- **Hilos**: el monitor exclusivo es de un solo hilo.
- **FPCR**: se asume la configuración por defecto (redondeo al más cercano, sin flush-to-zero ni NaN por defecto). FPSR no lleva las banderas de excepción ni QC.
- **Velocidad**: el código traducido es entre 2 y 4,5 veces más lento que el nativo. Los registros viven en memoria y todavía no hay optimizaciones.
- **Memoria**: la del juego se mapea en sus mismas direcciones dentro del proceso, sin aislamiento.

## Hoja de ruta hacia un juego real

1. ~~NEON y punto flotante~~: hecho, verificado contra Unicorn.
2. **Tablas de salto y descubrimiento de código** robusto para binarios grandes.
3. **Optimización**: registros en variables locales y eliminación de banderas que nadie lee.
4. **Formato NSO y NSP**: descompresión LZ4 y los módulos `rtld`, `main` y `sdk`. La extracción con `prod.keys` sacadas de tu propia consola puede reutilizar las herramientas de nxnative.
5. **Hilos y sincronización de Horizon**: mapear los hilos del juego a hilos de Linux, con atómicos de verdad.
6. **Servicios del SDK** (`sm`, `fs`, `hid`, `vi`, `nvdrv`, audio). nxnative tiene muchos en C portable bajo `dm/host`.
7. **Gráficos**: NVN sobre Vulkan para la RTX/Radeon, en lugar de Metal.
8. **Integración con Omarchy**: un lanzador `.desktop`, mandos con SDL y ventana en Hyprland.

Los juegos y las llaves tienen que ser tuyos, sacados de tu propia consola. Este repositorio no incluye nada de ningún juego.

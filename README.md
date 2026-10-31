# nxrecomp

Prueba de concepto: convertir código de Nintendo Switch (ARM64) en ejecutables **nativos de Linux x86_64**, sin emulador.

La idea es la de [nxnative](https://github.com/seedds/nxnative), llevada a una PC con Linux. nxnative puede ejecutar el código del juego tal cual porque los chips de Apple son ARM64. Un Ryzen o un Intel no lo son, así que aquí el código se traduce **una sola vez, antes de jugar** (recompilación estática, como N64Recomp o XenonRecomp). Al jugar no hay intérprete ni JIT: corre un programa normal de Linux.

## Qué hace hoy

```
guest/hola.c (ARM64) ──clang──▶ hola.elf ──mknro──▶ hola.nro ──nxrecomp──▶ hola_rec.c ──clang──▶ build/hola (x86_64)
```

1. `guest/` es un programa de prueba en formato homebrew de Switch: cabecera NRO, MOD0 y un `svc` para hablar con el kernel de Horizon.
2. `tools/mknro.py` lo empaqueta como `.nro`.
3. `recomp/nxrecomp.py` lee el NRO, descubre las funciones y traduce cada instrucción ARM64 a C (`recomp/a64.py`).
4. `runtime/` carga la imagen en la misma dirección que en la Switch, arma la pila y el TLS, y responde las llamadas al kernel (`svcOutputDebugString`, `svcGetSystemTick`, `svcExitProcess`, `svcBreak`).

```
make test
```

compila el programa para la Switch, lo recompila y compara su salida con la del mismo código compilado directo para x86_64. Pasa con `-O0`, `-O1`, `-O2`, `-O3`, `-Os` y `-Oz` (`make test OPT=-Os`).

## Qué cubre el traductor

Todo el conjunto de enteros de A64:
- aritmética y lógica con banderas NZCV;
- desplazamientos, campos de bits (BFM/UBFM/SBFM) y EXTR;
- multiplicación y división con la semántica de ARM (dividir entre 0 da 0);
- cargas y guardados en todas sus formas, incluidos pares y exclusivas (LDXR/STXR);
- saltos directos, condicionales e indirectos;
- registros de sistema (TPIDR/TPIDRRO, contador de 19,2 MHz, NZCV, FPCR/FPSR) y `DC ZVA`.

Lo que no cubre todavía se traduce a `nx_trap`, que detiene el programa y dice qué instrucción falta y dónde.

## Límites conocidos

- **SIMD/FP (NEON)**: no está. Por eso el programa de prueba se compila con `-mgeneral-regs-only`. Los juegos reales lo usan en todos lados.
- **Tablas de salto** de los `switch`: un `BR` a una etiqueta dentro de la función todavía no se resuelve. Por eso la prueba usa `-fno-jump-tables`.
- **Hilos**: el monitor exclusivo es de un solo hilo y faltan los atómicos LSE.
- **Velocidad**: el código traducido es unas 4 veces más lento que el nativo. Los registros y las banderas viven en memoria y todavía no hay optimizaciones.
- **Memoria**: la del juego se mapea en sus mismas direcciones dentro del proceso, sin aislamiento.

## Hoja de ruta hacia un juego real

1. **NEON y punto flotante**: los registros V0-V31 y las cientos de variantes SIMD.
2. **Tablas de salto y descubrimiento de código** robusto para binarios grandes.
3. **Optimización**: registros en variables locales y eliminación de banderas que nadie lee.
4. **Formato NSO y NSP**: descompresión LZ4 y los módulos `rtld`, `main` y `sdk`. La extracción con `prod.keys` sacadas de tu propia consola puede reutilizar las herramientas de nxnative.
5. **Hilos y sincronización de Horizon**: mapear los hilos del juego a hilos de Linux, con atómicos de verdad.
6. **Servicios del SDK** (`sm`, `fs`, `hid`, `vi`, `nvdrv`, audio). nxnative tiene muchos en C portable bajo `dm/host`.
7. **Gráficos**: NVN sobre Vulkan para la RTX/Radeon, en lugar de Metal.
8. **Integración con Omarchy**: un lanzador `.desktop`, mandos con SDL y ventana en Hyprland.

Los juegos y las llaves tienen que ser tuyos, sacados de tu propia consola. Este repositorio no incluye nada de ningún juego.

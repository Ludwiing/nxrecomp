# nxrecomp

Prueba de concepto: convertir código de Nintendo Switch (ARM64) en ejecutables **nativos de Linux x86_64**, sin emulador.

La idea es la de [nxnative](https://github.com/seedds/nxnative), llevada a una PC con Linux. nxnative puede ejecutar el código del juego tal cual porque los chips de Apple son ARM64. Un Ryzen o un Intel no lo son, así que aquí el código se traduce **una sola vez, antes de jugar** (recompilación estática, como N64Recomp o XenonRecomp). Al jugar no hay intérprete ni JIT: corre un programa normal de Linux.

## Qué hace hoy

```
guest/*.c (ARM64) ──clang──▶ .elf ──mknro──▶ .nro ──nxrecomp──▶ _rec.c ──clang──▶ build/<programa> (x86_64)
```

1. `guest/` tiene programas de prueba en formato homebrew de Switch: cabecera NRO, MOD0 y llamadas `svc` al kernel de Horizon.
2. `tools/mknro.py` los empaqueta como `.nro`.
3. `recomp/nxrecomp.py` lee el NRO, descubre las funciones (y los destinos de las tablas de salto, en `recomp/jumptable.py`) y traduce cada instrucción a C. El decodificador está en `recomp/a64.py` (enteros y memoria), `recomp/simd.py` (FP escalar y cargas SIMD) y `recomp/neon.py` (NEON y cripto).
4. `runtime/` carga la imagen en la misma dirección que en la Switch, arma la pila y el TLS, y responde las llamadas al kernel. También tiene la semántica exacta de ARM para punto flotante (`nxfp.h`), NEON (`nxsimd.h`) y cripto (`nxcrypto.h`).

```
make test
```

compila cada programa para la Switch, lo recompila y compara su salida con la del mismo código compilado directo para x86_64:
- `hola`: enteros. Pasa en `-O0` a `-Oz` (`make test OPT=-Os`).
- `vm`: una máquina virtual de bytecode (un `switch` de 32 casos dentro del bucle del intérprete), un analizador léxico y switches densos y dispersos. Ejercita las tablas de salto en `-O0` a `-Oz`.
- `multi`: dos módulos NSO comprimidos con LZ4, `main` y `sdk`, enlazados entre sí como el ExeFS de un juego. Tiene llamadas por PLT/GOT, tablas de punteros, una vtable, constructores, un símbolo débil ausente y una importación sin definir.
- `neon`: punto flotante y bucles vectorizados con `-O3`.
- `hilos` (comparado con `guest/esperado/hilos.txt`): el kernel. Usa el mismo protocolo de mutex que libnx (LDAXR/STLXR y `svcArbitrateLock`). Tiene 4 hilos con mutex y 4 con contadores atómicos, productor y consumidores con variables de condición, ping-pong con `WaitForAddress`, eventos, esperas con plazo, heap, una pila mapeada con `MapMemory`, `QueryMemory` y `GetInfo`. Estable en 70 corridas seguidas, también en un solo núcleo.
- `servicios` (comparado con `guest/esperado/servicios.txt`): IPC con mensajes CMIF armados como los de libnx. Habla con `sm:`, `set` y `fsp-srv`, convierte una sesión en dominio, recibe objetos nuevos como id o como handle, usa buffers B, cierra un objeto y pide un servicio que no existe. Usa FMUL/FADD, TBL, ST3, SQXTN2, SHRN2, UMLAL2, UZP2, REV64, CNT, ADDV, BSL, FCVTZS y más. Los resultados coinciden bit a bit.

## Juegos: NSP → ExeFS → C → ejecutable

```
python3 tools/extraer.py --keys ~/.switch/prod.keys juego.nsp [--update actualizacion.nsp] -o ~/Juegos/juego
python3 recomp/nxrecomp.py ~/Juegos/juego/ExeFS -o ~/Juegos/juego/c
python3 tools/compilar.py ~/Juegos/juego/c -o ~/Juegos/juego/juego
```

- **Extracción** (`tools/extraer.py`): usa `nx_extract` de nxnative (LibHac, licencia MIT, en `tools/nx_extract`). Abre NSP, NSZ, XCI y XCZ, junta la actualización con el juego base y saca el ExeFS y el RomFS. Necesita el SDK de .NET (`sudo pacman -S dotnet-sdk`) y las `prod.keys` de tu propia consola.
- **Módulos** (`recomp/nso.py`): lee NSO con LZ4 y verifica el SHA-256 de cada segmento, igual que el cargador de Horizon. También lee NRO y el `main.npdm` (pila, prioridad, espacio de direcciones).
- **Enlace** (`recomp/program.py`): hace el trabajo de `rtld`, pero antes de traducir. Ubica `rtld`, `main`, `subsdk*` y `sdk` uno detrás de otro, aplica las reubicaciones (RELA, JMPREL y RELR empaquetadas) y resuelve los símbolos entre módulos con las reglas de Horizon. Así el traductor ve las direcciones finales de las vtables y de la GOT. Lo que ningún módulo define queda como un stub que dice su nombre si alguien lo llama.
- **Salida**: el C se reparte en varios archivos y la imagen de cada módulo se incluye con `.incbin`, sin copiarla al C. `tools/compilar.py` los compila en paralelo.

## Homebrew reales con libnx

Con devkitPro instalado (`tools/instalar-devkitpro.sh`, que necesita sudo y se corre después de `omarchy update`), `make test` también compila y prueba dos homebrew hechos con libnx 4.12 sin tocar:
- `guest/libnx/basico`: el arranque completo de libnx (`crt0`, reubicación, heap, `sm`, `set:sys`, `appletOE`, `hid`, `time`, `fs`), `main` y la salida por `ISelfController::Exit`.
- `guest/libnx/sistema`: las API de libnx como las usa cualquier homebrew: hilos con `Mutex` y `CondVar`, idioma, versión del sistema, relojes y calendario, archivos y carpetas en la SD con `fopen`/`opendir`, mandos con `padUpdate` y `malloc` de 64 MiB.

Además corren dos ejemplos oficiales de devkitPro con pantalla, sin tocar: `printing/hello-world` (la consola de texto de libnx) y `simplegfx` (dibuja píxeles directo en el framebuffer). `make test` compara la huella del décimo cuadro.

Para probar uno nuevo: `tools/probar_libnx.sh <nombre>` lo compila, lo recompila con `--traza` y lo corre. Para depurar:
- Con `NX_TRAZA_SVC=1` se ven las llamadas al kernel.
- Con `NX_TRAZA_FUNC=1`, cada función que se ejecuta. `aarch64-none-elf-addr2line` traduce la dirección a función y línea.
- Si el programa falla con una violación de segmento, el runtime muestra la dirección que falló, la última instrucción ARM y los registros.

## El kernel y los servicios

Como en nxnative, el código del SDK de Nintendo (o el de libnx) corre traducido tal cual. Lo que se reemplaza es lo que está debajo: el kernel de Horizon y los servicios del sistema.

- **Kernel** (`runtime/nxkern.c`): cada hilo del juego es un hilo de Linux. Sigue la semántica del kernel de Atmosphère (mesosphère) para hilos, mutex (`ArbitrateLock`/`Unlock`), variables de condición, árbitro de direcciones, eventos, esperas, heap, `MapMemory`, memoria física de la región alias, `QueryMemory` y `GetInfo`. Las instrucciones LDXR/STXR son compare-and-swap de verdad, así que los atómicos del juego funcionan con varios hilos.
- **IPC** (`runtime/nxipc.c`): mensajes CMIF con cabecera especial, handles, buffers A/B/X/C, comandos de control y dominios.
- **Servicios**:
  - `sm:`, `set` (idioma: `es-419` por defecto, o `NX_IDIOMA`), `set:sys` (sistema 18.1.0) y `apm`, en `runtime/nxservices.c`.
  - `appletOE`/`appletAE`, en `nxsvc_am.c`. La consola está en la base, a 1080p y con el foco.
  - `hid`, con su memoria compartida, en `nxsvc_hid.c`. Por ahora sin mandos conectados.
  - `time:u/a/s`, con la memoria compartida de los relojes, la hora real y la zona horaria del anfitrión, en `nxsvc_time.c`.
  - `fsp-srv`, en `nxsvc_fs.c`. La SD es una carpeta: `NX_SDMC`, por defecto `~/.local/share/nxrecomp/sdmc`. El RomFS es una imagen (`NX_ROMFS`).

  - `vi:u/s/m` (pantalla y capas), el binder con la cola de buffers de Android (`IGraphicBufferProducer`) y `nvdrv` con `/dev/nvmap`, en `nxsvc_vi.c`. Cada cuadro encolado pasa por `runtime/nxdisplay.c`: se reordena del formato "block linear" de la GPU, se convierte a RGBA y se muestra en una ventana SDL3 a 60 cuadros por segundo. Variables:
    - `NX_SIN_VENTANA=1`: no abre ventana.
    - `NX_CAPTURA=f.ppm`: guarda el cuadro número `NX_CAPTURA_CUADRO` (30 por defecto) y termina.

  Si un programa pide un servicio o comando que falta, se avisa con `[RED]` y su nombre.

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

- **Arranque de un juego real**: después de los constructores se salta directo a `nnMain`. Un juego con el SDK de Nintendo necesita antes la secuencia de `nn::init::Start` (`nnosInitialize`, `nninitStartup`...), y esa secuencia necesita hilos, SVC y servicios de Horizon. Es el siguiente paso.
- **Formatos**: todavía no se leen los segmentos ZBIC (zstd, sistema 22.0.0 o posterior) ni las reubicaciones de TLS. Si aparecen, se avisa.
- **Tamaño**: con un `main` de decenas de MB, el recompilador (en Python) y la compilación del C van a tardar. Falta medirlo con un juego.
- **Servicios**: faltan los mandos de verdad (llenar la memoria de `hid` con SDL), la GPU (NVN sobre Vulkan, la parte más grande; hoy solo se muestran cuadros dibujados por la CPU), `audren`/`audout`, `acc` (cuentas), las partidas guardadas y otros. El orden de arranque del SDK de Nintendo (`nn::init::Start`) está en `runtime/nxmain.c`, pero sin probar con un juego real.
- **Excepciones de C++ y `longjmp`**: un RET a una dirección que no es inicio de función (el destino de un `longjmp`, o una plataforma de aterrizaje de una excepción) todavía no se resuelve.
- **FPCR**: se asume la configuración por defecto (redondeo al más cercano, sin flush-to-zero ni NaN por defecto). FPSR no lleva las banderas de excepción ni QC.
- **Velocidad**: el código traducido es entre 2 y 4,5 veces más lento que el nativo. Los registros viven en memoria y todavía no hay optimizaciones.
- **Memoria**: la del juego se mapea en sus mismas direcciones dentro del proceso, sin aislamiento.

## Hoja de ruta hacia un juego real

1. ~~NEON y punto flotante~~: hecho, verificado contra Unicorn.
2. ~~Tablas de salto~~: hecho. `recomp/jumptable.py` reconstruye el cálculo del destino de cada `BR` hacia atrás (incluso con la dirección de la tabla sacada del bucle o el índice pasado por la pila en `-O0`), encuentra la cota del `CMP` y lee la tabla. Los destinos se vuelven etiquetas `goto` de la misma función.
3. **Optimización**: registros en variables locales y eliminación de banderas que nadie lee.
4. ~~Formato NSO y NSP~~: hecho (extracción, NSO con LZ4, enlace entre módulos, NPDM).
5. **Hilos y sincronización de Horizon**: mapear los hilos del juego a hilos de Linux, con atómicos de verdad.
6. **Servicios del SDK** (`sm`, `fs`, `hid`, `vi`, `nvdrv`, audio). nxnative tiene muchos en C portable bajo `dm/host`.
7. **Gráficos**: NVN sobre Vulkan para la RTX/Radeon, en lugar de Metal.
8. **Integración con Omarchy**: un lanzador `.desktop`, mandos con SDL y ventana en Hyprland.

Los juegos y las llaves tienen que ser tuyos, sacados de tu propia consola. Este repositorio no incluye nada de ningún juego.

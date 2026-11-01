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

Además corren dos ejemplos oficiales de devkitPro con pantalla, sin tocar: `printing/hello-world` (la consola de texto de libnx) y `simplegfx` (dibuja píxeles directo en el framebuffer). `make test` compara la huella del décimo cuadro. `guest/libnx/mandos` comprueba los mandos: un guion aprieta botones, combinaciones, la cruceta y + para salir.

Hay dos ejemplos de audio:
- `playtone` (`audout`) toca 220, 440 y 1760 Hz con A, B y X, y se compara el WAV.
- `audren-simple` (el renderizador) tiene que sacar su `sample.bin` intacto, muestra por muestra, en los dos canales.

Y dos de la GPU, con deko3d (el driver de bajo nivel de los homebrew modernos): `deko_basic` arranca el dispositivo, crea las colas, sube sus shaders y dibuja su triángulo de colores; `deko_console` dibuja una consola de texto con una textura de fuente (un arreglo 2D de 256 capas subido con el motor de copia) y 3600 instancias por cuadro. `make test` comprueba varios píxeles de cada uno.

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

  - Mandos, en `runtime/nxinput.c`: el teclado y los mandos de SDL se escriben en la memoria compartida de `hid`, como un mando Pro en el jugador 1 y en modo portátil.
    - Teclado (como el predeterminado de Ryujinx): stick izquierdo WASD, cruceta con las flechas, A/B/X/Y en Z/X/C/V, L/ZL en E/Q, R/ZR en U/O, stick derecho IJKL, + en Enter, − en Retroceso.
    - Mandos: por posición, como en la Switch.
    - Pruebas: `NX_PULSAR="30:plus,45:a+b"` aprieta botones en cuadros fijos.

  - Audio, en `runtime/nxaudio.c` (salida SDL3), `nxsvc_audio.c` (`audout`) y `nxsvc_audren.c` (el renderizador `audren`, el que usan los juegos).
    - El renderizador mezcla 240 muestras cada 5 ms. Lee voces en PCM de 8/16/32 bits, float y DSP-ADPCM, con remuestreo lineal, factores de mezcla por canal, submezclas y salida estéreo o 5.1.
    - `NX_SIN_AUDIO=1` no abre el dispositivo.
    - `NX_AUDIO_WAV=f.wav` graba lo que suena.

  - La GPU, en `runtime/nxgpu.c`, con sus dispositivos de `nvdrv` (`nvhost-ctrl`, `nvhost-ctrl-gpu`, `nvhost-as-gpu`, `nvhost-gpu`) en `nxsvc_vi.c`:
    - Espacio de direcciones de la GPU (las direcciones de GPU apuntan a la memoria del juego), syncpoints con esperas reales, y el GPFIFO: cada `SubmitGpfifo`/`KickoffPb` ejecuta el pushbuffer en el momento.
    - Motor 3D Maxwell (`B197`): el intérprete de macros MME completo (los drivers suben sus macros y las llaman), `ClearBuffers` sobre render targets lineales o "block linear", semáforos de reporte, carga en línea (`I2M`) y carga de constbufs.
    - Shaders: `runtime/nxshader.c` traduce el código de máquina Maxwell a GLSL 4.60. La codificación de las 293 instrucciones (`runtime/mxisa.h`) se genera con `tools/gen_maxwell.py` desde las tablas del traductor de Ryujinx (MIT, `third_party/LICENSE.ryujinx`); la semántica está escrita aquí. Ya traduce atributos, constbufs, aritmética de punto flotante y entera (incluida XMAD), bits, conversiones, predicados, memoria local, `discard` y el flujo de control (saltos, SSY/SYNC, PBK/BRK, PCNT/CONT, CAL/RET, como un `switch` dentro de un bucle). `tools/mxdis.py` desensambla.
    - Dibujo: `runtime/nxgl.c` sobre OpenGL 4.6 sin ventana (EGL), en la GPU de la máquina. Cada draw arma el estado de OpenGL desde los registros (shaders traducidos y cacheados, constbufs, atributos de vértice, índices, viewport, scissor, caras, profundidad, blending, máscaras) y dibuja en texturas que reflejan los render targets del juego: se cargan desde su memoria cuando la CPU pudo cambiarla y se escriben de vuelta (con su "block linear") antes de cada syncpoint o semáforo.
    - `NX_TRAZA_GL=1` muestra los shaders traducidos y cada draw; `NX_TRAZA_GPU=1`, cada método del motor 3D; `NX_SIN_GPU=1` no dibuja.
    - Cómo se verifica: `make test` compila los GLSL de `tests/shaders` con `uam` (el compilador de deko3d) a código Maxwell, los traduce de vuelta y compara, píxel por píxel en float32, contra el GLSL original en la misma GPU.
    - Texturas: los handles del constbuf de texturas llevan al descriptor de imagen (TIC) y al de sampler (TSC). Se leen el formato (RGBA8, sRGB, float, enteros, BC1-BC7, profundidad...), el tipo (2D, 3D, cubo, arreglos), los mipmaps y el swizzle; se deshace el "block linear" de cada nivel y se suben a OpenGL con una caché que vuelve a mirar la memoria en cada envío. Un render target que se lee como textura se usa directo de la GPU. El traductor cubre TEX, TEXS, TLD, TLDS, TLD4, TLD4S y TXQ.
    - Motor de copia (DMA): copias entre memoria lineal y "block linear", con remapeo de componentes.
    - Las instancias que los drivers dibujan una por una se juntan en un draw instanciado.
    - Falta: ASTC (la mayoría de los juegos lo usa y las GPU de PC no lo leen), texturas enteras, half floats, motores 2D, de cómputo y de copia, profundidad sincronizada con la memoria, geometría y teselación, y un backend Vulkan.

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
- **Audio**: el ADPCM, el remuestreo con tono distinto de 1 y las submezclas están escritos, pero ningún ejemplo los prueba todavía. Faltan los efectos (reverb, delay...) y los formatos de revisión más nuevos que usa el SDK de Nintendo.
- **Servicios**: la GPU ya dibuja con shaders traducidos sobre OpenGL, pero sin texturas todavía (ver arriba), y faltan `acc` (cuentas), las partidas guardadas y otros. El orden de arranque del SDK de Nintendo (`nn::init::Start`) está en `runtime/nxmain.c`, pero sin probar con un juego real.
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
7. **Gráficos**: NVN sobre Vulkan para la RTX/Radeon, en lugar de Metal. Hechas las dos primeras etapas: los comandos de la GPU llegan, las macros corren, los shaders se traducen y se dibuja con OpenGL. Siguen las texturas.
8. **Integración con Omarchy**: un lanzador `.desktop`, mandos con SDL y ventana en Hyprland.

Los juegos y las llaves tienen que ser tuyos, sacados de tu propia consola. Este repositorio no incluye nada de ningún juego.

# Lo que falta antes del primer juego

Sacado de la experiencia de nxnative con unos 125 juegos reales (`~/src/nxnative/docs`). nxnative
reemplaza funciones del SDK (NVN → Metal); nxrecomp trabaja más abajo, como Ryujinx: corre el driver
NVN del propio juego y ejecuta sus pushbuffers. Por eso parte de sus fallos no aplica aquí, pero lo que
falla por cómo funciona el hardware sí. Orden: lo que más probablemente rompe un primer juego, primero.
Se marca [x] lo hecho.

## GPU

1. [x] Estado del pushbuffer entre entradas del GPFIFO: los draws indirectos de NVN ponen los
   argumentos de la macro en otra entrada. Semáforos completos: acquire (=, >=, and), release y
   reducciones atómicas, en el puller y en el motor 3D. (Falta: esperar syncpoints.)
2. [~] Motor de copia y 2D rápidos: [x] tabla de páginas (sin candado ni recorrido), [x] copias por
   tramos de 16 bytes, [x] mapeos vecinos contiguos, [x] semáforo del LaunchDma; el 2D en la GPU, con conversión de formato y profundidad.
3. [~] Coherencia: [x] vigilancia de memoria por protección de páginas (nxrt.c, NX_SIN_VIGIA=1 la
   apaga): una textura o un render target se vuelve a leer solo si la CPU escribió alguna de sus
   páginas desde la última lectura (antes se hasheaba todo en cada envío y se recargaban todos los
   render targets); las lecturas de archivos abren las páginas antes del read(). Prueba: la textura
   que la CPU reescribe en deko_gpu. [x] Los render targets se devuelven a memoria solo cuando la
   CPU (o el runtime) los toca: en el syncpoint su memoria se esconde (PROT_NONE) y el primer acceso
   la copia desde la GPU, dentro del manejador de la señal. [ ] caché de buffers en vez de subir todo en cada draw; storage
   buffers de tamaño 0.
4. [~] Reportes: hecho el tipo de contador (consultas de OpenGL), CounterReset y las marcas de
   tiempo en ticks de 614,4 MHz. [x] Renderizado condicional (RenderEnable, sin prueba: deko3d no lo expone). Antes: tipo de contador (oclusión, primitivas, transform feedback) y marcas de tiempo en
   ticks de la GPU (ns × 384 / 625). Renderizado condicional (RenderEnable).
5. [~] Render target leído como textura: [x] vistas de OpenGL con otro formato del mismo tamaño,
   como arreglo o con swizzle; [x] glTextureBarrier al leer render targets; si no se puede ver así,
   se escribe a memoria y se lee de ahí. Falta: niveles de mipmap, caras de cubo y capas dibujados
   y leídos en el mismo envío; codificadores BC que dibujan sobre la memoria de la textura (agl).
6. [x] Handles de textura y sampler separados (OR de dos palabras de constbuf, LopR/LopC/Lop3);
   texturas de buffer (descriptor tipo 6 → samplerBuffer, sin filtro y con ancho de 32 bits).
7. [~] Estado fijo que va dentro del shader: [x] alpha test, [x] posición invariante, [x] atributos
   que la etapa anterior no escribe = (0,0,0,1), [x] render targets que el shader no escribe quedan
   intactos, [x] draws sin render target, [x] rasterizador apagado. Falta: early-Z forzado,
   alpha-to-coverage.
8. [x] Render targets en capas (arreglos con gl_Layer desde vértices, teselación o geometría),
   limpieza por capa (ClearBuffers bits 10-20), profundidad en capas, vistas de arreglos.
9. [ ] Transform feedback.
10. [~] Raster y samplers: [x] logic op, [x] alpha test, [x] depth clamp (ViewVolumeClipControl),
    [x] cubos sin costuras, [x] reducción min/max (TSC palabra 1, bits 10-11, confirmado con
    deko3d). Falta: blend avanzado, viewport swizzle, coordenadas sin normalizar, texturas
    multisample y PIXLD, lectura de stencil.
11. [ ] Origen abajo-izquierda completo (gl_FragCoord.y, scissor, SR_Y_DIRECTION).
12. [ ] Etapas y registros: geometría *passthrough*, VertexA, primitive ID, máscaras de hilos.
13. [~] [x] Memoria sparse: Remap (trozos de 64 KiB de distintos buffers, o vacíos), copias DMA que
    cruzan mapeos y huecos (leen ceros), texturas repartidas en varios mapeos (se juntan en un buffer).
    Prueba en deko_gpu. [x] Estado por canal: cada /dev/nvhost-gpu abierto tiene sus
    registros, macros, motores y lector de pushbuffer (sin prueba propia: deko3d vuelve a fijar todo
    el estado en cada lista, así que no se nota con sus ejemplos).
14. [~] Presentación: [x] intervalo de swap (30 fps), [x] recorte y espejos del QueueBuffer.
    Falta: nvdec/vic para videos; presentar directo desde la textura de la GPU (hoy se deshace el
    block linear en la CPU en cada cuadro).

## CPU y recompilador

1. [~] Saltos indirectos a código no visto: [x] el runtime los anota en NX_PISTAS y
   `nxrecomp.py --pistas` los agrega como funciones (NX2Linux ofrece recompilar al salir el juego).
   [x] Tablas de salto con la base lejos (1024 instrucciones hacia atrás); el resolvedor ya seguía
   índices copiados antes de la cota, máscaras, cmp de 64 bits, tablas de bytes y bases en bucles.
2. [~] [x] QueryMemory por segmento (Code RX, Code R, CodeData RW). [ ] Excepciones de C++ y longjmp
   (necesitan continuaciones en los landing pads y en los puntos de retorno: diseño pendiente).
3. [~] [x] Barrera después de STLR. [ ] Accesos a memoria del juego como atómicos relajados antes de
   optimizar registros.
4. [x] Llamadas de cola garantizadas: saltos directos a otra función con `musttail`, y los
   indirectos (BR sin tabla, RET a otra dirección) por puntero con nx_buscar + `musttail`.
5. [~] Escala: medido con deko_examples (≈300 KB de código: 2,5 s, 76 MB, 8,4 MB de C, unos 20 s
   de clang); proyectado a 60–100 MB: minutos de Python y 10–20 min de clang en 16 núcleos. Caché del
   resolvedor de tablas acotada. [ ] Probar con un main real.
6. [ ] NRO cargados con nn::ro (ldr:ro), ZBIC, relocaciones TLS.

## Kernel

1. [x] ID del hilo en TLS+0x110 (SDK 22).
2. [x] GetInfo 16/17 (recurso del sistema del NPDM), 18 (program ID del ACI0 o NX_TITLE_ID), 20
   (contexto de excepción), 25 (ticks de CPU del hilo), 26.
3. [x] SetHeapSize: "sin memoria" (0xD001) por encima del límite.
4. [x] WaitForAddress tipo 3 (64 bits), SignalToAddress tipo 2 con cuenta ≤ 0 (−2).
5. [x] SetThreadActivity / GetThreadContext3: pausa con una señal, contexto desde el Ctx.
6. [x] Número de procesador del núcleo del hilo; prioridad y núcleo del hilo principal del NPDM.
7. [x] SVC 0x22, 0x24, 0x2A/0x2B, 0x30/0x31, 0x36.
   Prueba: guest/libnx/kernel (guest/esperado/kernel.txt).

## Servicios

1. [x] `acc:u0`/`acc:u1`: un usuario (apodo de NX_USUARIO o del de Linux), perfil y avatar; la cuenta
   de Nintendo responde "no disponible". `am`: usuario preseleccionado (PopLaunchParameter tipo 2).
2. [x] Partidas guardadas en `fsp-srv` (51/52/53 abren y crean la carpeta; cuenta, dispositivo,
   caché, temporal y sistema por separado; 22-36 aceptan; lectores de guardados vacíos; 202 abre los
   DLC por su id); `am` 25/26/27.
3. [x] Stubs por tabla (nxsvc_otros.c): friends, prepo, pctl, olsc, npns, ssl, bcat, nfp/nfc, irs,
   caps, ldn, mii, lm, mm, audin, spl, notif, ectx, pdm, sfdnsres, nifm (IRequest en espera y después
   sin red), aoc (DLC de NX_AOC), fatal. Con implementación: pl:u (una fuente TTF del anfitrión en las
   6 del sistema), csrng, bsd (todo falla con ENETDOWN; Poll y Select duermen), ngc.
4. [x] `am`: applets que cierran al momento (selector de usuario, teclado, mandos, errores) con sus
   eventos, IStorage, notificaciones vacías, idioma desde NX_IDIOMA.
5. [x] `hid` (evento de estilos ya avisando), `time` (zonas 1-6, ClockSnapshot 400/401, 300, 500,
   CalculateSpanBetween), `apm` (overclock), `set` (región, mapa de teclas, apodo), `set:sys`/`set:fd`
   (tema, ajustes de depuración, autor de Mii).
6. [~] Audio: [x] revisiones del SDK (REV1–REV15, como Ryujinx), tamaño de la memoria de trabajo, IAudioDevice,
   AUX, captura, biquad, limitador, splitters, remuestreador de Nintendo, 32 kHz. Prueba: guest/libnx/audren_rev.
   [x] `hwopus` con libopus (uno y varios flujos). [x] `audout`: Stop devuelve los buffers.
   [ ] delay, reverb, reverb 3D y compresor (hoy dejan pasar la señal).
7. [x] Herramienta que lista los servicios (tools/servicios.py) que nombra el juego antes del primer arranque.
   Prueba: guest/libnx/cuentas (guest/esperado/cuentas.txt). NX_TRAZA_IPC=1 muestra cada comando.

## App NX2Linux

1. [x] Agregar una actualización o un DLC a un juego ya instalado ("Actualización o DLC" en la página del juego,
   o `nx2linux --agregar <title id> <archivos>`). DLC: se extrae a `aoc/` sin recompilar. Actualización: vuelve a
   extraer y reconstruye; el recompilador solo reescribe los archivos de C que cambiaron.
2. [x] Elegir el prod.keys al agregar cada juego (el juego lo recuerda); la app propone los de las carpetas habituales.
3. [x] Motores especiales: un juego con port propio se construye y se abre con él (TotK 1.2.1/1.4.3: totk-pc-recomp).
4. [x] SSBU con ssbu-recomp como motor especial (13.0.2, 13.0.4, 13.0.5); el que ya estaba construido se registró sin
   reconstruir (`registrar_construido`).
5. [~] NVN en el host + backend Vulkan genérico (el camino para cualquier juego NVN). [x] Capa NVN de totk-pc-recomp en
   runtime/nvn/ con backend nulo: TotK llama 183 funciones NVN (pools, texturas, colas, grabaciones, presentación).
   [x] Núcleos virtuales en el kernel (los motores guardan datos por núcleo sin mutex: dos hilos del mismo núcleo no
   pueden correr a la vez), con preferencia en SVC, llamadas indirectas y por señal (SIGURG). TotK pasa CallCommands.
   [ ] Bucles de espera activa (rt_spin_gate de totk-pc-recomp). [x] Backend Vulkan de totk-pc-recomp (runtime/nvn/,
   ventana de nxdisplay.c): TotK por el camino genérico dibuja su introducción. [x] Vigilancia de escrituras de la CPU
   (userfaultfd + PAGEMAP_SCAN sobre el heap y la región alias). [x] Un hilo mejor toma el núcleo de uno que está dentro
   del runtime (sin válvulas ni carreras). [x] -O1 con las funciones de más de 5000 líneas en optnone: TotK compila en
   17 min y va a 30 fps estables (su tope en la Switch). [ ] Congelar regiones (mem_freeze).
   [x] Traductor de shaders bajo demanda (totk_xlate; NX2Linux pasa TOTK_XLATE y la caché del juego).

## Switch 2

La CPU es un Cortex-A78C (ARMv8.2 con RCpc, producto punto y autenticación de punteros); la GPU es
Ampere (NVN2). Se avanza por capas, de lo más acotado a lo más grande. Pruebas de CPU:
`tests/fuzz.py --cpu switch2` (Unicorn con la CPU "max" de QEMU) y `tests/lse_test.c`.

1. [x] Atómicos LSE (ARMv8.1): CAS/CASP, LDADD/LDCLR/LDEOR/LDSET, LD{S,U}{MAX,MIN}, SWP en todas sus
   variantes y tamaños; LDLAR/STLLR; LDAPR (ARMv8.3). Unicorn compara sin signo LDSMAX/LDSMIN de
   8/16/32 bits: esos casos van en lse_test.c.
2. [x] Autenticación de punteros, desactivada (como con SCTLR_EL1.EnIA = 0): PACIASP y compañía son
   hints vacíos, PAC*/AUT*/XPAC* dejan el registro igual, BRAA/BLRAA/RETAA (y las Z y B) son saltos
   normales y LDRAA/LDRAB usan la base tal cual. Unicorn trae la autenticación apagada, así que la
   clase `pac` del fuzzer la compara exacta. Falta PACGA (calcula una firma; no se ha visto usada).
3. [x] NEON de ARMv8.1/8.2: SQRDMLAH/SQRDMLSH y SDOT/UDOT (vector, escalar y por elemento; clase
   `neon82`). Aritmética FP16 escalar (clase `fp16`) y vectorial (clase `fp16v`: tres iguales, dos
   registros, reducciones, por pares, por elemento, FMOV inmediato y punto fijo), con los mismos
   helpers que 32/64 bits instanciados para `_Float16`. Unicorn extiende el signo en FCVTZS Hd, Hn,
   #fbits: ese caso se salta. No están FCMLA/FCADD/FJCVTZS (ARMv8.3 opcionales que el A78 no tiene).
4. [~] Formato de los ejecutables de Switch 2. Hecho: NSO con compresión ZBIC (bit 7 de las banderas,
   sistema 22.0.0+ y Switch 2; zstd 1.5.7 con magia `ZBIC` y tablas de entropía BIC) vía el paquete
   `zbic` de kinnay (GPLv2, de Atmosphère) instalado con `make pydeps` en `.pydeps/`; NPDM con espacio
   de 42 bits, Flags1 (pila sombra), RemoteMemorySize y ACID de RSA-3072 (0x180 bytes; el ACI0 se busca
   siempre por los offsets del META). `tests/nso_test.py` reempaqueta `multi` en ZBIC y exige el mismo C.
   Falta: el contenedor de los juegos de Switch 2 y los servicios nuevos (sin documentación pública ni un
   dump propio para probar), y la carga de wkc0-wkc9 (LoadBrowserCoreDll, bit 7 del ACID).
5. [ ] GPU: NVN2, clases de Ampere y traductor de shaders SASS de Ampere (lo más grande).

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
   Contenedor `.nup` (dumps de Switch 2 ya descifrados): `tools/nup.py` (propio, MIT), y NX2Linux lo acepta.
   Falta: los servicios nuevos (sin documentación pública ni un
   dump propio para probar), y la carga de wkc0-wkc9 (LoadBrowserCoreDll, bit 7 del ACID).
5. [ ] GPU: NVN2, clases de Ampere y traductor de shaders SASS de Ampere (lo más grande).
6. [x] Detección Switch 1 / Switch 2 (`recomp/nso.py` plataforma: ACID RSA-3072, RemoteMemorySize; 42 bits y ZBIC
   solos no alcanzan). NX2Linux la anota en juego.json y avisa en el registro. Prueba: `tests/plataforma_test.py`.
7. [~] Programas de NVN2: `runtime/nvn/nvn2_program.h` lee la sección de control (magia 0x98761234, largo del código,
   c1, etapa, grupo de trabajo; instrucciones de 16 bytes). Prueba sintética: `tests/nvn2_program_test.c`. Falta
   confirmar con un juego real los anchos de etapa y grupo de trabajo y la numeración de las etapas.
8. [ ] Reloj de la GPU de Switch 2 (31,25 MHz en vez de 19,2) en los contadores de tiempo (vk_exec_xfer.c
   write_counter), cuando exista NVN2.

## Switch 2 completo en NX2Linux, en MIT (plan pedido el 2026-11-08)

Todo escrito para nxrecomp, sin copiar código de Hookshot (GPL-3.0): solo ideas y formatos. Las pruebas con juegos
reales las hace el usuario con sus dumps y trae el registro; aquí se prueba con programas y datos sintéticos.

- [x] Fase 1: HLE por símbolo. `nxrecomp.py --sustituir` se puede repetir y acepta `símbolo funcion_c`,
  `símbolo =VALOR`, `símbolo !` (devuelve 0 y avisa una vez: runtime/nxhle.c) y prefijos `nn::...*`.
  `recomp/sustituciones-switch2.txt` se agrega solo en juegos de Switch 2. `tools/sdk_simbolos.py <ExeFS>` lista las
  funciones nn:: y nvn del juego por grupo y marca las que faltan. Prueba: `tests/sustituir_test.py`.
- [ ] Fase 2: kernel y servicios de Switch 2 por HLE: nn::os (hilos, fibras, sincronización), nn::gs
  (BootGraphicsSystem), nn::syncpt, nn::vi de Switch 2, nn::fs, nn::hid, nn::audio, nn::oe/account/time.
  Se llena sustituciones-switch2.txt con los nombres que dé sdk_simbolos.py sobre un dump.
- [ ] Fase 3: NVN2 sobre runtime/nvn (versión 231.x de la API, tabla de límites del dispositivo, program builders,
  imágenes nativas, pools virtuales de 64 bits, mesh/task).
- [~] Fase 4: traductor propio de SASS Ampere (SM86, instrucciones de 16 bytes); lector de programas ya hecho
  (runtime/nvn/nvn2_program.h). Método, sin código GPL: nvdisasm del kit CUDA (`sudo pacman -S cuda`, solo en la PC de
  desarrollo) como oráculo. `tools/sm86_oraculo.py` desensambla y enumera los códigos (334 válidos, 134 instrucciones,
  con todas las de gráficos: IPA, OUT, AL2P, ISBERD, KILL, PIXLD, TEX, TLD, TLD4, TXD, TXQ, TMML);
  `tools/sm86_campos.py` cambia cada bit de cada código y anota qué operando o modificador cambia (mapa de campos).
  `tools/sm86.py generar` arma la especificación (campos de cada operando, tablas de modificadores probando todas sus
  combinaciones, y un mapa propio para cada modificador que cambia la cantidad de operandos: 59) y `tools/sm86.py
  validar N` compara con nvdisasm instrucciones al azar: 1863/2000 (93 %) iguales. Fallan sobre todo las de medio
  float (HADD2, HMUL2, HFMA2, HSET2: inmediatos de dos halfs y valor absoluto de constantes), IMMA/HMMA, algunas formas
  de TLD y LDG. Falta: llevar el decodificador a C en el runtime y la bajada a lo que traduce runtime/nxshader.c.
  - [~] Bajada de lo uniforme y de las constantes con registro (`tools/sm86_baja_uniformes.py`): UMOV, ULDC (todos
    los tamaños), S2UR, R2UR, VOTEU (ALL/ANY/EQ), UPLOP3 y LDC (c[b][Ra+off], .IL/.IS/.ISL, todos los tamaños).
    Diseño: URn en R(188+n), URZ = RZ; como URZ no ocupa registro, R251 es UPR, el registro de predicados
    uniformes (bit n = UPn, UPT constante). Una guarda uniforme @UPn toma prestado Pn: P2R guarda Pn en el bit 8+n
    de UPR, R2P carga UPn en Pn, el cuerpo va con @Pn y otro R2P devuelve Pn (las instrucciones uniformes no tocan
    P). Leer un UP: BFE de UPR; escribirlo: BFI en UPR. Quien arma el programa reserva R188..R251 y R252..R254.
    La comprobación ejecuta la instrucción SM86 y lo bajado (con la semántica de Ryujinx) en un warp de 32 hilos.
    El "off_reg" de la especificación no es un registro: es el bit 53, el bit 15 del offset de c[b][off]
    (nvdisasm lo muestra con signo); se toma sin signo (bancos de 64 KiB). c[b][URx+off] no existe en SM86.
    Falta:
    - cx[URn][off] (bit 91: el banco lo da un registro uniforme, en ALU, MOV y ULDC). En Turing/Ampere es una
      constante sin enlace (bindless): el UR lleva un mango del runtime (dirección y tamaño del búfer), no un número
      de banco. Diseño: (1) saber de NVN2 qué guarda el compilador en ese UR (si fuera un número de banco, basta
      LDC.IS con Ra = (UR << 16) | off, que Ryujinx ya traduce con banco variable); (2) si es un mango, el runtime
      tiene que dar una tabla mango -> búfer y el traductor leer de un SSBO con esa tabla (Ryujinx no lo tiene).
    - REDUX (reducción del warp a UR): con SHFL.BFLY en mariposa (5 pasos) solo es exacta con el warp entero
      activo; hace falta una forma exacta (p. ej. por VOTE + FLO para el primer hilo activo) o apoyo en el traductor.
    - R2UR con predicado de salida y R2UR.OR (OR de todos los hilos activos).
    - UIADD3, ULOP3, USEL, USHF, UFLO, UPOPC, ULEA, UISETP, UPRMT... y las formas de las instrucciones comunes con
      un UR como operando no están en build/sm86_spec.json: el decodificador no las reconoce, así que no hay
      regla posible hasta que la especificación las tenga (las reglas serían las comunes sobre R(188+n)).
    - `probar` de sm86_baja.py solo cambia los bits de los operandos, no los de los modificadores: no prueba, p. ej.,
      ULDC.64, VOTEU.ANY ni LDC.IS. Se probaron aparte (cambiando también esos bits): 2699/2699 iguales.
  - [~] Bajada de texturas y entradas/salidas (`tools/sm86_baja_texturas.py`): TEX, TLD, TLD4, TXD, TXQ, TMML, IPA,
    AL2P, ISBERD, OUT, KILL, PIXLD. La disposición de registros de las texturas se sacó compilando PTX con ptxas
    (sm_86) y mirando qué valor queda en cada registro: está escrita en la cabecera del módulo. Resumen:
    destino partido (Rd = componentes 1 y 2, Rd2 = 3 y 4; Maxwell los quiere seguidos: MOVs a TEMPS); fuentes con la
    lista lógica de Maxwell pero repartidas parejo entre Ra y Rb cuando son 4 o menos (Ra 1 con 1-2, 2 con 3-4); TXD
    y TXQ como Maxwell; mango ligado en c[banco][off] (bits 54-58 y 40-53). Las 58 texturas distintas que sacó ptxas
    (2D/3D/cubo/arreglos, LZ/LL, AOFFI, DC, MS, bindless, TLD4 con PTP/DC, TXD, TXQ) bajan y se comprueban bien; al
    azar (cambiando también tipo y modificadores, que `probar` no toca), 0 diferencias. La comprobación ejecuta lo
    Maxwell en símbolos con el orden de lectura de Ryujinx (InstEmitTexture.cs) y compara fuentes, destinos y el
    resto de los registros; KILL se prueba con los 128 valores de P0..P6.
    Falta / decidir con NVN2 real:
    - Banco de los mangos: `BANCO_TEXTURAS = 2` (el de NVN 1 que totk_xlate supone). Si el juego usa otro banco para
      todas sus texturas, quien arma el programa cambia BANCO_TEXTURAS (y totk_xlate su TexSlot); si mezcla bancos,
      las de otro banco ya se bajan como bindless con MOV c[banco][off] (Ryujinx las vuelve ligadas con ese cbuf_slot),
      pero eso pide temporales: con 4 o más fuentes en un lado queda SinRegla, y TMML no se puede (Ryujinx no resuelve
      TMML bindless).
    - Formas sin significado confirmado (SinRegla): las de tres inmediatos (0xb62, 0xb65, 0xb68, 0xb6b, 0xb6e, 0xb71,
      solo válidas con .B: ¿índices de un pool?) y .B en las formas ligadas (banco y offset con .B).
    - Sin equivalente en Maxwell/Ryujinx: .F16 (resultado en medio float), .LC/.FDV (clamp de LOD de TEX y TXD),
      predicado de residencia (sparse), TXD 1D/ARRAY_1D (ptxas los pasa a 2D: disposición no vista), IPA con
      predicado en 81-83 o a[UR+off], ISBERD .U16/.32/.SKEW, PIXLD sin modo o .INNER_COVERAGE, OUT.FINAL.
    - Ryujinx no implementa PIXLD (se baja igual, al Pixld de Maxwell) ni los modos de muestra de IPA (CENTROID,
      OFFSET: se pasan en Msi/SrcC y los ignora). IPA.PASS de Ampere se toma igual que el de Maxwell (sin MULTIPLY: el
      compilador multiplica por 1/w con FMUL); falta confirmarlo con un shader de fragmento real.
    - ALD/AST no están en build/sm86_spec.json: el generador armó la forma base con Ra = R0, que en ALD solo vale con
      .P o .PHYS. nvdisasm sí decodifica 0x321: `ALD Rd, a[imm], Rvtx` (Ra = RZ), `ALD.P Rd, a[Ra+imm]`,
      `ALD.PHYS Rd, a[Ra], Rvtx`; Rd 16-23, Ra 24-31, Rvtx 32-39, imm 40-50 (11 bits con signo), tamaño .64/.96 en
      74-75, .P 76, .PHYS 77, .O 79: los mismos campos que el ALD de Maxwell (la bajada sería directa). AST (0x322) no
      salió con ninguna combinación probada. Para que haya regla hay que regenerar la especificación con esas formas
      (tools/sm86_oraculo.py ALTAS / tools/sm86_campos.py con Ra = RZ). No se vio que los atributos de vértice vayan
      por LDS/LD sobre una ventana: si un shader real lo hace, quien arma el programa tiene que reconocer esa ventana
      (dirección base en una constante) y cambiar esos LDS/LD por ALD/AST.
    - `probar` de sm86_baja.py no cambia los bits de tipo (61-62) ni los de los modificadores: en TLD la forma base
      queda sin LZ/LL (INVALID0) y casi todo sale "forma indefinida". Se probaron aparte.
  - [~] Saltos, memoria y programa completo (`tools/sm86_baja_saltos.py`, `tools/sm86_programa.py`): BRA, BRX,
    CALL.REL/RET, BAR.SYNC, MEMBAR, LDG/STG/LD/ST, LDS/STS, LDL/STL, ATOM/ATOMG/RED, ATOMS; BSSY/BSYNC/BREAK/WARPSYNC/
    YIELD/NANOSLEEP/DEPBAR/ERRBAR/CCTL/BMOV a NOP (no a SSY/SYNC: en el modelo por hilo de Ryujinx no cambian nada; el
    porqué está en la cabecera del módulo). Los saltos devuelven un marcador `Reubicar` que sm86_programa.py resuelve.
    `python3 tools/sm86_baja_saltos.py probar 4000` (también las formas con descriptor uniforme que no están en la
    spec, contra el texto de nvdisasm): 2750/3360 iguales, 610 SinRegla a propósito (casi todas desplazamientos al
    azar que no caben en Maxwell), 0 distintas.
    `tests/sm86_programa_test.py` (make test): programas SM86 a mano -> Maxwell -> totk_xlate -> spirv-val; los de
    cómputo corren en la GPU y dan lo esperado (saltos, lazo, BRX con tabla, CALL/RET, LDC, compartida, BAR, STG).
    Falta / decidir con NVN2 real:
    - Cabecera: se copian las palabras 0-19 de la SPH de 128 bytes (mismo diseño que Maxwell); las 20-31 se descartan
      con aviso si no son cero: falta ver qué guardan en un shader real.
    - Registros: el shader no puede usar R188-R254 (URn, UPR, TEMPS); hoy es un error, falta reasignar registros. El
      ancho de los operandos se estima por el nombre (.64/.128/texturas).
    - BRX: solo con la tabla en c[0x1] (LDC de 32 bits + IMNMX.U32 con tope). El c1 que sale reescrito (desplazamientos
      Maxwell) es el que tiene que enlazar el runtime, no el del juego.
    - Sin regla: CAS (Ryujinx no implementa AtomCas/AtomsCas), dirección global + UR, STS/STL/ATOMS con UR,
      desplazamientos de atómicos de más de 20 bits (LD/ST genéricos: 24), F64 atómico, dirección global de 32 bits
      con descriptor, BRA.INC/.DEC, CALL.ABS, BAR.ARV/RED/SCAN.
    - Supuestos: BRA.DIV nunca salta y BRA.CONV siempre (warp convergido); RET vuelve a quien llamó (se ignora su
      registro); LD/ST genéricos son globales; la caché/alcance (.CONSTANT, .STRONG.GPU, .EF...) se ignora.
  - [~] Enteros de 64 bits, IMAD, desplazamientos y conversiones (`tools/sm86_baja_enteros.py`): IADD3 con P de
    acarreo y .X, ISETP.EX, LEA/LEA.HI (.X y P), IMAD en todas sus formas (.MOV/.IADD/.SHL son solo nombres de
    nvdisasm; .WIDE, .HI, .X), SHF (L/R, 32/64, .W, .HI), LOP3 y FLO con predicado de salida, BMSK, SGXT, BREV, IDP
    (4A, 2A.LO/HI), I2I.SAT, I2F/I2FP, FRND (F16/F32/F64), F2I y F2F.
    Acarreos: se quedan en el predicado de Ampere y cada instrucción se baja sola (no hay CC vivo entre dos
    instrucciones SM86: quien arma el programa no las junta ni cuida su orden, solo reserva R252..R254). Producir
    un acarreo = comparación sin signo (a + b se pasa si la suma < b; a - b si a >= b; a + ~b si a > b; a + p si
    a == -1 y p), con la cuenta en un registro cuando hace falta (P = cuenta >= 1, Q = cuenta >= 2: la suma P + Q es
    la del hardware, el reparto entre P y Q no se ve con nvdisasm). Consumirlo = SEL 0/1 sumado como un operando más.
    P -> CC solo como borrador dentro de ISETP.EX (SEL + R2P.CC -> ISETP.X de Maxwell). Por qué no CC: Ryujinx ignora
    CC/.X en IADD3, IMAD, LEA y SHF, XMAD.X no suma C, y el CC de IADD con un negado vale mal cuando el negado es 0.
    Comprobación: COMPROBAR ejecuta la instrucción SM86 (semántica supuesta) y lo bajado (semántica de Ryujinx, sacada
    de su código) sobre estados al azar con bordes; las conversiones se comparan campo a campo con el nombre de
    nvdisasm. `python3 tools/sm86_baja_enteros.py probar 5000` (cambia también los bits de modificadores y variantes):
    2926/4761 iguales, 1835 SinRegla, 0 distintas; con `--oraculo` los modificadores leídos de la spec coinciden con
    nvdisasm salvo en formas con ??? (la spec junta tokens repetidos). `probar` general solo con este grupo:
    1734/3000, 0 distintas (el resto SinRegla: casi todo cx[UR], F2IP y formas base con ???).
    Falta:
    - F2I (0x305, 0x311), F2F (0x304, 0x310), FRND de 16/32 bits (0x307) e I2F desde 64 bits o a F64 (0x312), con sus
      formas c[] (0xb..), no están en build/sm86_spec.json: el generador prueba las partes altas de ALTAS
      (tools/sm86_oraculo.py) y estas solo decodifican con bits de formato puestos: 0x101000 (F2F/F2I), 0x201000
      (FRND), 0x300000 (F2I.F64), 0x301400 (F2F.F64), 0x1c00 (I2F.F64). Las reglas ya están; con una especificación
      provisional (SM86_SPEC_EXTRA=..., generada con sm86_campos.mapa + sm86._spec_de) dan 195/195 iguales. Hay que
      agregar esas partes altas a ALTAS y regenerar la spec (dueño: el decodificador).
    - `decodificar()` nombra solo el primer grupo de modificadores que cambia (SHF.R.U32.HI sale sin HI, IDP.2A sale
      4A, I2F pierde el redondeo...). Este módulo lee los bits crudos; las reglas que usen d["nombre"] en
      instrucciones de varios grupos están expuestas.
    - Sin regla: IMAD.WIDE con predicado de salida (acarreo de 64 bits: faltan temporales), LEA.HI con -A y acarreo
      o con -B, F2IP (nvdisasm no dice cómo empaqueta A, B y C), F2F a BF16 (Maxwell no lo tiene), FRND con
      inmediato doble, I2F/I2FP/FRND/LEA con ???.
    - Ryujinx: F2I a 64 bits ("Unimplemented 64-bits F2I") e I2F desde 64 bits ("TODO: Handle S/U64") no los
      traduce; F2F solo redondea si fuente y destino son del mismo formato; I2I.SAT satura sin signo (por eso I2I va
      con dos IMNMX); SHF de 32 bits con desplazamiento >= 32 da 0 en vez de C/A (se satura y se usa el SHF largo).
    - Supuestos que nvdisasm no puede confirmar: P de FLO = no hay bit; P de IMAD.HI = acarreo sin signo de
      hi(A*B) + C; LEA.X con ~A = ~(A << s); LEA.HI -A = parte alta de -({C:A} << s); IDP: el primer tipo es el de A;
      BMSK A = inicio y B = ancho; SGXT con ancho 0 da 0; .NTZ de F2I se ignora.
    - LOP3/LOP/LEA de Maxwell escriben DestPred: si no se da, codificar() deja 0 y Ryujinx pisa P0 (con F: falso).
      Le pasaba al LOP3 de sm86_baja.py (este módulo lo reemplaza) y le pasa al UPLOP3 de sm86_baja_uniformes.py.
- [~] Fase 5: hecho el acomodo de texturas de Switch 2 (GOB con los bits 5 y 6 cambiados, NX_PLATAFORMA=switch2 que
  pone NX2Linux; tests/gob_test.c). Falta: ASTC en la GPU, mesh shaders, caché de shaders de NVN2.
- [ ] Fase 6: por juego, parches de resolución, FPS y pantalla ancha (como los de SSBU y TotK).

Ideas de NX2navi (tangerineturtle1/NX2navi; solo su primer commit es MIT, el resto es "todos los derechos
reservados": se leen las ideas, no se copia código). Su runtime viene, como el nuestro, de totk-pc-recomp.
- Shaders: bajar SM86 a Maxwell (propio: tools/sm86.py) y traducir con el traductor de Ryujinx (MIT) que nxrecomp ya
  tiene como totk_xlate, en vez de runtime/nxshader.c.
- AMD FSR 3.1 (MIT): escalado, antialiasing y generación de cuadros con el HUD fuera del cuadro generado; reemplazaría
  a vk_framegen.c.
- Mesh/task con VK_EXT_mesh_shader; los dibujos indirectos leen el registro {count, first} de NVN con un anillo de
  {x, y, z} llenado en la GPU.
- VK_EXT_graphics_pipeline_library: enlace rápido de pipelines nuevas y la optimizada en segundo plano.

## Generación de cuadros (Switch 1 y 2)

- [x] `TOTK_FRAMEGEN=on` (vk_framegen.c, runtime/shaders/fg_*.comp): con el juego entre 10 y 40 fps, un cuadro
  intermedio por flujo óptico (luma a 1/4, búsqueda simétrica por bloques de 16x16 px, mediana 3x3, cada píxel elige
  entre los vectores vecinos) se muestra a mitad de camino. Prueba en la GPU: `tests/framegen_test.c`.
- [ ] Probarla en un juego NVN real (hasta ahora solo con cuadros sintéticos).
- [ ] Bordes que se tapan o destapan: queda una estela corta en el borde que avanza.


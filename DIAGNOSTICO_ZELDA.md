# Estado del diagnóstico local de Zelda (2026-11-03)

El juego todavía no llega al menú. No se ha sustituido el ejecutable instalado.
Clutch permanece cerrado y su desarrollo está pendiente del arranque de Zelda.

Se corrigieron IPC TIPC/CMIF, sesión de dominio, notificación de foco inicial,
parcel de NativeWindow, características de GPU y devolución de buffers registrados.
Se generó una copia Nintendo RomFS de la imagen DMROMFS1, conservando el original.

La nueva corrección del recompilador conserva el retorno nativo entre llamadas de
cola mediante nx_enter/nx_tail (campos añadidos al final de Ctx). Evita despachar
continuaciones del llamador como funciones nuevas cuando LR contiene una llamada
intermedia o se reutiliza como registro temporal. El código generado previamente
se actualiza desde tools/branch_local.py al compilar.

Pruebas: make test-startup pasa. retorno_br y servicios recompilados producen la
salida esperada exacta. La prueba de cola incluye un millón de saltos con -O0 y -O2.

Se canceló la recompilación -O1 por lentitud mediante Ctrl-C de la sesión. La
recompilación -O0 con 16 hilos terminó: 289 archivos. Los objetos de c/obj son ahora
-O0 con el nuevo Ctx; la próxima compilación -O2 los invalidará correctamente.

Artefactos de diagnóstico (no distribuidos): /tmp/zelda-ipc-diagnosis/
- game-tail-clean: candidato completo, con 15 funciones adicionales reales de
  tablas de salto; excluye los 13 parches antiguos de continuaciones incorrectas.
- tail-missing.txt: direcciones reales pendientes de descubrimiento, todas
  validadas contra ExeFS y sin instrucciones BL/BLR inmediatamente anteriores.
- tail-run.log: último arranque, termina con SIGSEGV escribiendo a dirección 0.
- run-tail.py: enlaza objetos y ejecuta con guardados temporales, timeout 60 s.
- extend-tail.py: hasta 8 iteraciones de extensión estática; no está ejecutándose.

Fallo actual: memset recibe destino nulo. ARM LR=0x8e1d970, llamada en 0x8e1d96c
(f_ab1b250 -> f_d35f580), dentro del cuerpo generado f_8e1bb08. x24 es nulo y se
copia a x0 para memset; inspeccionar la asignación previa de x24 y su reserva de
memoria. Fuente: c/funciones_076.c alrededor de L_8e1d96c. Traza completa del último
fallo en tail-run.log; ya supera los errores de IPC y de retorno originales.

GAME/c/pistas_extra.c contiene las 15 funciones reales; su objeto actualizado está
en /tmp/zelda-ipc-diagnosis/tail-extra.o y en c/obj/pistas_extra.c.o. GAME/pistas.txt
se actualizó con las 15 pistas reales; se retiraron las 13 pistas antiguas incorrectas.
El usuario no tenía pistas propias antes.

Las instalaciones globales solicitadas después (ECC y herramientas adicionales)
están bloqueadas: ~/.codex es de solo lectura y la shell no resuelve GitHub. Se
prepararon scripts en el directorio home para ejecutarse desde la terminal normal.

## Avance posterior: ya no se reproduce el cierre inicial

Cambios confirmados adicionales:
- nvmap ya no tiene límite de 256 buffers: metadatos en lista con direcciones
  estables y publicación atómica. Prueba de 4096 buffers simultáneos pasa.
- IDs exportados de nvmap comparten el namespace de handles del proceso. NVN
  reutiliza el pool entre los dos buffers de pantalla y antes fallaba en el segundo
  AcquireTexture (ID 0x107 frente a handle 7; devolvía índice -1).
- HID 203 ahora entrega IActiveVibrationDeviceList y admite ActivateVibrationDevice.
- IStorage RomFS usaba fseeko + fread sobre el mismo FILE compartido entre hilos:
  carrera confirmada reproduciendo diferencias con el código original. Ahora usa
  pread, y flockfile en el caso fmemopen. 64000 lecturas concurrentes pasan.
- R_AM_NO_DATA corregido de 0x2a8 a 0x480 (módulo AM 128, descripción 2). El SDK
  abortaba al no reconocer el resultado de PopLaunchParameter de un canal vacío.
- Estado inicial de VertexB (reg 0x810) activo, 0x11, también en shadow RAM. NVN
  escribe su offset 0x811 sin configurar el enable. Antes se saltaban todos los VS.
- nx_bad_jump tiene backtrace opcional con NX_TRACE_BOOT, al igual que SIGSEGV.

Con 23 funciones adicionales reales la ejecución sin instrumentación permanece
60 segundos sin cerrarse. Sin embargo, la imagen sigue negra: NO está arreglado
el renderizado y NO se sustituyó el ejecutable instalado. La captura del cuadro
120 anterior a VertexB también es negra. deko_basic recompilado con los cambios
pasa la comprobación de sus 6 píxeles de referencia (cuadro 5).

Prueba adicional de las funciones de libc ARM del SDK, enlazando el juego en un
harness: memcpy, memmove y memset pasan 8193 casos cada una (24579 en total),
conservan x19-x29 y SP. Harness y log en /tmp/zelda-ipc-diagnosis/check-memory*.

Investigación gráfica actual:
- Sin VertexB no se traducía VS. Con default 0x11 ya se traducen VS y FS y se
  ejecutan draws; las muestras del RT siguen sin color.
- EGL en la sandbox usa llvmpipe OpenGL 4.6; no se ha probado todavía en la ventana
  normal con acceso de la aplicación anfitriona a la GPU física.
- Draw inicial de quad: 6 índices, 1 instancia, topología visible GL_TRIANGLES.
  VBO stride 16, attr0=0x38600000 (4 half floats). Posiciones son (+/-0.5,+/-0.5,0,0).
  CB3 tiene filas de matriz: escala X/Y=2, Z constante -1, W constante 1.
- ViewVolumeClipControl=0x181d, clip 0..1 activo; cull activo, CCW/back, sy=-540.
  Máscaras de color 0x1111 (correctas), raster enable=1.
- Seguían apareciendo algunas cabeceras de pushbuffer inválidas (0xc4070000,
  0xd74e00xx). Las primeras entradas del GPFIFO sí tienen paquetes y clases válidas.

Instrumentación PRIVADA en /tmp/zelda-ipc-diagnosis/run-tail.py:
- TAIL_EXTRA=1 incluye tail-extra.o (23 funciones; c/obj/pistas_extra.c.o aún tiene
  una versión anterior hasta la próxima compilación de esa fuente).
- TRACE_GRAPHICS / TRACE_BITS / TRACE_TIMER / TRACE_NVN / TRACE_ACQUIRE activan
  overrides o wrappers específicos; no deben instalarse como versión del juego.
- TRACE_GL_INPUTS=1 sustituye nxgl.c.o por gl-inputs.o (copia modificada en /tmp).
  Esta copia registra VBO y CB y captura gl_Position por transform feedback.
- DIAG_NO_CULL y DIAG_CLIP_NEGATIVE son wrappers diagnósticos y NO correcciones.
- La primera instrumentación de transform feedback era incorrecta: relink sin
  shaders retenidos, luego EndTransformFeedback en una sola rama. Ambas cosas se
  corrigieron en gl-inputs.c. No se usaron sus resultados inválidos como fixes.
- Última prueba terminada: TRACE_GL_INPUTS=1 DIAG_NO_CULL=1 DIAG_CLIP_NEGATIVE=1,
  captura cuadro 30 en zelda-raster-diag.ppm. Transform feedback válido mostró
  seis posiciones que forman el quad: (-1,1,-1,1), (-1,-1,-1,1), (1,1,-1,1),
  (1,1,-1,1), (-1,-1,-1,1), (1,-1,-1,1). La geometría llega al final del VS.
  La captura sigue completamente negra incluso con los dos controles raster
  diagnósticos. Revisar salida del fragment shader, texturas, blend y scissor.
  Exit 0 corresponde a la captura automática, no a un arranque normal verificado.
  Resultado en capture-raster-diag-result.txt. El candidato game-tail-clean ES
  SOBRESCRITO por cada prueba: regenerar SIN overrides antes de instalarlo.

Los extractores de funciones /tmp/zelda-ipc-diagnosis/extract-function.py y
map-crash.py evitan recorrer todos los GB de C para localizar una función.
La aplicación NX2Linux sigue abierta y Jugar está disponible. Se observó por
Computer Use; aún no se pulsó Jugar con un ejecutable nuevo.

Pausa solicitada: primero plugins de Codex y guardián de instalaciones; después
continuar Zelda. ECC 2.2.3 ya aparece habilitado. Instalaciones adicionales
pendientes: ~/.codex y ~/.agents están montados como solo lectura en esta sesión.
Preparado ~/instalar-plugins-adicionales-codex.sh para la terminal del usuario.
El guardián local tiene 8 pruebas unitarias superadas, pero SkillSpector aún no
está instalado. No confundir instrucciones persistentes con un motor activo.

## Retoma: compilación visible y observación de textura

El usuario pide priorizar Zelda y aprovechar toda la CPU. La app muestra una
construcción nueva en curso, 54 %, fase Compilar a x86-64. motor.py invoca
compilar.py sin -j: usa os.cpu_count(), 16 en esta máquina, con -O2. No se
reinició ni se detuvo la construcción. vmstat mostró 92 % user + 8 % system,
0 % idle y 16 tareas ejecutables; swap ocupada ~19 GiB y lecturas de swap.
No aumentar por encima de 16 ni iniciar otra recompilación concurrente.

Prueba independiente del binario diagnóstico anterior, sin reenlazar contra
objetos que la construcción actual está modificando: LD_PRELOAD de
/tmp/zelda-ipc-diagnosis/shader-observe.so, captura cuadro 3 con saves temporales.
Terminó exit 0 por captura. shader-0000-8b30.glsl: FS simple que copia
texture(tex0, UV) a frag0. glScissorIndexed cubre 1920x1080. Unidad 128 enlaza
textura 3 de 1920x1080; falta inspeccionar contenido y procedencia de esa textura.
Logs observe.log y observe-texture.log. No se modificó el runtime ni el
ejecutable instalado durante estas pruebas. El binario diagnóstico conserva
los overrides documentados de TF/clip/cull, por lo que no es candidato final.

## Reducción de carga y nuevo cierre (continuación)

Se cambió compilar.py: hasta 8 trabajos por defecto; -j sigue disponible.
Se pulsó Detener en NX2Linux y la interfaz confirmó la cancelación, conservando
los pasos extraer/recompilar. Problema observado: el coordinador viejo dejaba
clang huérfanos. CPU continúa alta; NO se ha reanudado otra compilación.
Se corrigió la cancelación del compilador para nuevas ejecuciones: SIGTERM/SIGINT
se propagan a los grupos de los clang y del enlazador; no se publican resultados
cancelados. Prueba con proceso hijo sleep verifica parada y limpieza de temporales.
Pruebas de caché, branch_local y make -j2 test-startup pasaron.
Se preparó ~/detener-compiladores-zelda.py, limitado a clang del usuario con
objetos temporales de este juego, para ejecutar en la terminal del usuario.
Esta sesión no ve los procesos anfitriones en /proc. Se pidió por pregunta
asíncrona el resultado; NO se ha recibido el número de procesos detenidos.

La reconstrucción del usuario había integrado solo 15 pistas y eliminado
c/pistas_extra.c. Se restauraron 23 pistas y generaron 8 funciones faltantes.
Prueba prolongada del binario antiguo, limitada a 2 CPU/2 hilos llvmpipe:
observe-long.log termina exit3 con destino faltante 0x89c937c, LR 0x89c9214.
Validado: destino en texto ejecutable, instrucción anterior B, destino B.cond.
Está dentro de una tabla de saltos: en 0x89c935c se arma base 0x89c936c,
0x89c9360 lee uint16 de x0+x13*2, 0x89c9364 suma offset*4 y 0x89c9368 BR x14.
Se añadió la pista real 0x89c937c: ahora pistas.txt y tail-missing.txt tienen 24.
La traducción adicional actual contiene 9 funciones y compiló a -O0. No se ha
enlazado ni instalado un ejecutable nuevo después de ese cambio.
La textura presentada es negra en origen: 0 de 2073600 píxeles RGB con color,
primer píxel 0,0,0,255 (observe-color.log). No garantiza que la GPU sea la causa:
el nuevo cierre muestra trabajo de CPU que aún no estaba completamente traducido.

## Retoma tras detener los 15 compiladores antiguos

El usuario ejecutó detener-compiladores-zelda.py y confirmó 15 detenidos.
Memoria disponible volvió a ~17 GiB. Se compiló juego.candidato a partir de los
C actuales (ahora 295 shards; 321 unidades totales) con -j8 -O0 y nuevo flag
--reutilizar-optimizados: permite reutilizar objetos frescos O1/O2/O3 en una
compilación O0, conservando las comprobaciones de fuentes/cabeceras/comando.
31 objetos O2 reutilizados, resto reconstruido a O0. Archivo instalado juego
NO se reemplazó; juego.json sigue con 2 pasos hechos, app detenida.
Se limita también LLD a args.j hilos. Tests compile_cache (ahora 2 casos) y
compile_cancel pasan. CPU de compilación medida ~53 %, RAM libre ~11 GiB.

NUEVA CORRECCIÓN GPU en runtime/nxgl.c: glClipControl y transformación Z consultaban
R[0x64F]&1 en lugar de DepthMode R[0x35F]&1. Caso reproducible: triángulo Z=-.5
con DepthMode=0 y control=0x181d desaparecía. tests/clip_depth_test.c falló antes
y pasa con la corrección (comprueba ambos modos). make test-gpu-state.
Referencia deko_basic recompilada, captura cuadro5 y 6 píxeles siguen pasando.
Fuente verificada envytools/rnndb/graph/gf100_3d.xml, 0x0d7c DEPTH_CLIP_NEGATIVE_Z.
Primera prueba candidato limpio24: timeout180sincrash; otra captura cuadro3
en10s, negra. Candidato depth llegó cuadro120 en21.5s, negro. Los tiempos y
bloqueos son variables: no afirmar arranque completo por una prueba corta.

NUEVA CORRECCIÓN eventos GPU: dumps de todos los hilos mostraron principal
esperando NvOsEventWait -> NvRmSync -> NVN. nx_nvdrv QueryEvent creaba un evento
nuevo sin relación con los syncpoints, que jamás se señalaba. Se implementó
tabla 64 NvSyncEvent, arm/reset/free/query con identidad estable; callback débil
nx_nvdrv_syncpt_changed desde nx_syncpt_incr DESPUÉS de soltar sp_lock; verifica
ID y threshold, soporta formatos de QueryEvent y clear codificado bit28.
No se han añadido incrementos artificiales ni se fuerzan señales.
Prueba tests/nv_sync_event_test.c enlaza nxgpu.c real, prueba umbral exacto, ID
equivocado, identidad de QueryEvent, clear/rearm/free, fast path y 128 carreras
registro vs finalización. Añadida a make test-startup.
La prueba inicial de events hizo bucle porque EventClear recibía 0x10000000;
se corrigió aceptar ese formato y conservar allocated en Clear.
La versión posterior avanzó 317 envíos GPU en22.9s y terminó exit3 por un NUEVO
destino estático omitido 0x89be480. Logs candidate-submit2.log.
Instrumentación temporal [nv-submit] bajo NX_TRAZA_NV imprime flags/increment/before.
Observado flags0x104, increment0/1/2; contador min sube1/2 según comandos. No tocar
fence flags aún sin evidencia de desajuste: en código upstream Eden distingue
max planificado y min ejecutado; bit8 increment_value solo modifica max.
Fuentes consultadas Switchbrew NV_services (ABI eventos) y Eden mirror nvhost_gpu.cpp.

Herramientas NUEVAS en /tmp/zelda-ipc-diagnosis:
- test-candidate.py --name NAME --timeout SEC --frame N: candidato limpio, saves
  temporales, affinity4CPU, LP_NUM_THREADS2, reporte JSON exit/missing/capture/color.
  Cada30s guarda NAME-threads.jsonl con estado/cpu_ticks/wchan por hilo.
  --dump-threads carga thread-dump.so (solo diagnóstico) y tgkill SIGUSR2 a todos
  los hilos al primer muestreo. thread-dump.c registra backtrace de cada hilo.
- candidate-stacks.log: dumps mapeados con nm del candidato entonces vigente.
  Principal: wait_sync -> SDK NvOsEventWait; workers esperan por dirección;
  audio thread en nx_audio_open -> SDL. No-watch prueba120s también se bloquea.
- advance-candidate.py: bucle ACOTADO a8 destinos faltantes. Inicial 0x89be480;
  valida texto/alineación/instrucción previa no BL/BLR/no destino repetido, añade
  a pistas.txt y tail-missing.txt, genera pistas_extra.c contra nxgame.h conocido,
  compila candidato reutilizando caché y prueba90s/cuadro600. Se detiene ante
  resultado distinto de exit3 o continuaciones/no código. NO instala candidato.
  ACTUALMENTE corriendo en sesión exec 27173: consultar salida con write_stdin.
  Cada ronda escribe advance-build-N.log y advance-N.log/reporte stdout.
- Se canceló candidate-submit (sin2) antes de usarlo porque se lanzó durante
  el enlace anterior; descartado. candidate-submit2 sí corresponde al enlace nuevo.

El problema visual y el arranque completo siguen SIN verificar. No marcarlo
resuelto ni sustituir juego aún. Mantener límite8 compilación y pruebas<=4CPU.

## Continuación: ubicación precisa del bloqueo y referencia ARM

- Ctrl ClearEvent recibió 0x10000000 (slot codificado); se acepta bit28 y se
  conserva allocated al limpiar. Tests 128 carreras de registro/finalización pasan.
- Logs NV opcionales nuevos: [nv-submit] flags/increment/before, [nv-event] arm/
  signal. nxkern.c tiene NX_TRACE_WAITS que imprime handles/timeout/id de hilo.
- candidate25-waits.log: principal hilo80 espera handle1080 del QueryEvent fd5
  nvhost-ctrl id10000000; contador18 queda209, espera210. Últimos envíos con
  incremento solicitado1/2 no incrementan el contador. No forzar el contador.
- nxgpu.c registra bajo NX_TRAZA_GPU el contexto de cabeceras inválidas y el
  estado pending al final de cada envío. candidate-pb.log tiene212 envíos del
  único canal4;210 terminan pending0, otros4551/4519. Primer error: entrada
  0x2d48e004c de19 palabras acaba con1 parámetro pendiente en método78e.
  La siguiente entrada es un paquete completo200406c0...; su cabecera se consume
  como parámetro, aparecen d74e0080/c4070000 como falsas cabeceras. c4070000 es
  float -540 de viewport. Investigar ensamblado/copia de comandos, no inventar
  resincronización ni aumentar arbitrariamente el tamaño del buffer.
- Herramienta privada trace-bind.c/.o enlazada con --wrap=nx_dispatch, sin tocar
  el juego instalado. game-bind-diagnostic usa objetos seleccionados EXACTOS de
  la compilación normal (no glob obj/*.o: hay objetos obsoletos allí).
  Captura GetProcAddress de NVN: BindBlendState d2557f0, EndRecording d21c530,
  SetViewport d247210. Capturas bind-N-ctx/object-before/after/state/commands.bin.
  End capturado DESPUÉS de primeros Bind, con páginas apuntadas por el objeto:
  bind-N-end-ctx/object-before/after/end-page-ADDR/end-after-page-ADDR.bin.
- reference-bind.py ejecuta función REAL ARM en Unicorn instalado en .venv:
  4 casos producen40 bytes idénticos y mismo avance del puntero que el C.
- reference-end.py ejecuta EndRecording REAL en Unicorn con memoria capturada:
  4 casos devuelven mismo handle, objeto idéntico y49/39/43/49 bytes modificados
  (valor exacto primer caso ver stdout) sin diferencias en metadatos. No se halló
  un fallo de traducción en esas dos funciones. No culpar a ARM antes de prueba.
- Más destinos reales omitidos: 89be480 y89be478 observados en ejecuciones. Se
  completó la familia de bloques mov-inmediato+B al mismo join:89be478/488/490,
  validados con ensamblado. pistas.txt y tail-missing.txt ahora28 entradas.
  pistas_extra.c13 funciones (15 ya integradas en shards), objeto O0 actualizado.
  Se reenlazó juego.candidato LIMPIO y game-bind-diagnostic con esos objetos.
- test-candidate.py acepta --binary PATH además de opciones previas.
- advance-candidate.py acepta destino inicial argv[1]; su ronda inicial25 acabó
  timeout90 sin nuevos faltantes y paró. No hay bucle activo.
- Prueba ACTUAL: candidate-copy, sesión exec89201, game-bind-diagnostic,45s,
  cuadro120. Wrapper ahora imprime APIs y before/after del puntero en
  nvnCommandBufferCopyCommands y CallCommands para localizar el salto perdido.
- Capturas headless siguen negras. No se ha instalado candidato ni validado
  menú/partida ni abierto ventana nativa nueva. Límite8 compilación,4 CPU pruebas.

### Continuación: entradas GPU estables
- Se reprodujo en test aislado una carrera: una notificación de finalización
  modificaba el segundo descriptor GPFIFO antes de que nx_gpu_submit lo leyera.
  También podía modificar palabras aún no interpretadas de una misma entrada.
- nxgpu.c copia los descriptores al entrar y las palabras por entrada antes de
  ejecutarla. Mantiene lectura diferida ENTRE entradas para argumentos escritos
  por comandos GPU previos. Test nuevo gpu_submission_snapshot_test falla antes
  y pasa después; también verifica generación de argumentos entre entradas.
- make -j2 test-startup test-gpu-state pasó, incluidas cancelación de compilador,
  eventos, profundidad y concurrencia RomFS. Solo nxgpu.c recompilado; enlace4.
- Candidato limpio candidate-snapshot llegó a120 cuadros en19.9s sin destinos
  faltantes, pero captura1920x1080 totalmente negra. Esto NO valida arranque.
- Ejecución candidate-snapshot-long (600 cuadros, límite90s) en sesión7673;
  NX_TRAZA_GPU/NV para comprobar si sigue la desalineación y espera210.
- La ejecución larga sí vuelve a desalinearse: primera cabecera inválida
  c4070000 en entrada2d48e004c palabra113/321. Antes se interpretan como
  parámetros680..687 los valores381,2,0,780,0,438,280,2; luego falta fence
  (syncpt18=182, esperado183), últimos pending6696/6664. La carrera corregida
  NO basta para el fallo principal. Mantener diagnóstico abierto, no instalar.

### Construcción terminada desde la app (15:47)
- A petición del usuario se pulsó Construir en NX2Linux. Perfil persistente
  c/compilacion.json: O0,4 hilos,reutilizar_optimizados=true. compilar.py admite
  este perfil por carpeta con validación; argumentos CLI tienen prioridad.
- App ejecutó compilación de321 unidades reutilizando caché y enlace correcto;
  registró compilar/terminar el2026-11-03 15:47:04 y creó acceso.desktop.
- Se verificó GUI100%,4 pasos completos y botón Jugar habilitado. Se reemplazó
  juego mediante enlace normal atómico. Ya no afirmar ejecutable intacto.
- Esto acredita CONSTRUCCIÓN, no arranque: aún pendiente fallo gráfico descrito.
  No se lanzó juego ni tocaron guardados en esta etapa.

### Ventana nativa abierta, pantalla negra
- Usuario abrió juego construido. list_windows confirma proceso juego PID364363,
  ventana The Legend of Zelda: Tears of the Kingdom, workspace3. Captura visual
  negra. ultima_partida.txt confirma RTX4060 Laptop/NVIDIA615.78.08/OpenGL4.6,
  no error de cierre. No se cerró su ventana ni tocaron guardados.
- Diagnósticos aislados privados, enlace4 y pruebas afinidad4: CallCommands
  d21dee0 comparado con ARM real en8 casos, objeto y bytes modificados iguales.
  SetViewport d247210 comparado con ARM en4 casos,36bytes iguales en cadauno.
- trace-malformed.c en /tmp captura APIs NVN que añaden paquetes no completos
  para acotar escritor original. Binario game-malformed-diagnostic, ejecución
  candidate-malformed sesión27174. No instalado.
- candidate-malformed terminó35.2s exit3 por destino86f543c sin traducir;
  no hubo paquetes incompletos capturados antes de ese punto. Dirección
  ejecutable validada: ADRP+ADD+B hacia86f5468, predecesor B (no BL).
  No se añadió todavía: no confundir su salida con diagnóstico GPU resuelto.
- candidate-viewport terminó19.9s120cuadros, captura negra; ARM viewport PASS4.
  La ventana nativa del usuario permanece abierta.

### Comparación solicitada con Smash
- Recuperado acceso tras borrar copias temporales que llenaban cuota /tmp.
- Smash localizado Projects/ssbu-recomp; launch.sh -> scripts/run.py --linux,
  build-linux/work/ssbu_native ELF. Zelda también ELF; diferencia NVN-HLE/Vulkan
  frente a NVN traducido+Maxwell/OpenGL. Ver COMPARACION_SMASH_ZELDA.md.
- Lectura código Smash nvn_gl.c cmd_end/cmd_call/queue_submit inspira auditoría
  de comandos grabados. trace-lifetime.c es PRIVADO y NO restaura datos.
  Primer prototipo usaba base en lugar de segmento; corregido a GPU=q11+q8-q6,
  CPU=q8,size=q0-q8. Diferencias observadas siguen sin demostrar causalidad.
- trace-submit.c captura entradas GPFIFO. pb0213..215 con170words terminan
  en20020280,44700000 (falta segundo argumento); pb0216 es paquete completo
  que se interpreta desalineado. Copias originales en/tmp.
- Se añadieron4 destinos ARM comprobados86f543c/5448/5454/5460, total32pistas,
  evitando solo completar repetidamente una misma familia. Hasta ahora solo
  juego.candidato, no instalado. candidate-extra32, sesión15524,240frames/60s.
- candidate-extra32 terminó por límite60s, sin destinos faltantes y sin
  alcanzar captura240. Todos los63hilos dormidos en muestra30s. No instalado.

### Selección OpenGL / Vulkan por Zink
- Cambios reales: nxmain.c configura NX_RENDERER=opengl|vulkan antes de inicializar
  drivers; Vulkan exige Mesa/Zink. nxgl.c usa EGL surfaceless y valida GL_RENDERER
  contenga zink; falla explícitamente si no inicializa, sin fallback oculto.
- motor.entorno_juego pasa renderer por juego. AjustesJuego ofrece OpenGL y
  Vulkan (Zink), explicando que es OpenGL sobre Vulkan, NO frontend NVN->Vulkan.
- tests/renderer_config_test.c PASS; make test-gpu-state PASS y py_compile UI PASS.
- Test Zink en sandbox falla VK_ERROR_INCOMPATIBLE_DRIVER: no hay /dev/dri ni
  /dev/nvidia aquí. Probe build/renderer_probe y acceso temporal
  nx2linux-renderer-probe.desktop preparados para comprobar GPU en escritorio.
  Pregunta asíncrona al usuario pendiente. No afirmar Vulkan validado.
- Se reconstruyó INSTALADO desde motor.Construccion desde compilar,4 hilos,35s.
  OpenGL predeterminado;32pistas registradas como usadas. nxdisplay.c después
  añade log del presentador SDL y exit4 en lugar de pausa infinita si falla;
  cambio solo en candidato de momento.
- Experimento privado trace-owned-list.c copia último descriptor NVN al cerrar,
  con nuevaVA/CPU. No sirve: malloc fuera39bits causó SIGSEGV; copia en39bits
  generó cabeceras inválidas y tamaños fusionados >mapeo. DESCARTADO, NO instalar.
- candidate-serialized (1CPU,120frames)45s captura negra,0destinos faltantes.
  candidate-extra32 y map-check sin cabeceras inválidas en esos recorridos,
  pero se atascan. No declarar probado que todos los bloqueos sean cabeceras.
- Nueva prueba candidate32-stall con NV/waits y dumps, sesión pendiente.
- candidate32-stall confirma espera syncpt18=52,umbral53; los últimos envíos
  anuncian increment1/2 pero min no cambia. Se capturan dumps para mapear.
  El silencio de cabeceras NO demuestra flujo válido; puede quedar pending.
- Vulkan sigue pendiente de prueba en GPU nativa. NO afirmar funcional por
  pasar renderer_config_test. No se ha seleccionado por defecto para Zelda.
- El experimento owned se descartó íntegro. No se añadieron ceros ni se
  forzaron syncpoints para simular finalización de comandos.
- Actualización final de juego mediante enlace temporal+os.replace incluye
  nxdisplay exit4/log presentación. Se retiraron game-owned-diagnostic,
  game-submit-diagnostic y temporalLLDfallido de/tmp para no agotar cuota.
- No hay pruebas de juego en ejecución ni compiladores dejados por esta etapa.
  Probe Vulkan del escritorio pendiente de respuesta del usuario.

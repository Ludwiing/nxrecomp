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
3. [ ] Coherencia perezosa: protección de páginas para saber qué escribió la CPU, devolver los render
   targets a memoria solo cuando la CPU los lee, caché de buffers en vez de subir todo en cada draw,
   storage buffers de tamaño 0.
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
13. [ ] Estado por canal (cada cola de NVN es un canal), espacio de direcciones con sparse y Remap,
    páginas sin mapear que leen 0.
14. [~] Presentación: [x] intervalo de swap (30 fps), [x] recorte y espejos del QueueBuffer.
    Falta: nvdec/vic para videos; presentar directo desde la textura de la GPU (hoy se deshace el
    block linear en la CPU en cada cuadro).

## CPU y recompilador

1. [~] Saltos indirectos a código no visto: [x] el runtime los anota en NX_PISTAS y
   `nxrecomp.py --pistas` los agrega como funciones (NX2Linux ofrece recompilar al salir el juego).
   [ ] Formas de tablas de salto que hoy no se reconocen (base lejos, índice copiado antes, máscaras
   de IL2CPP, cmp de 64 bits...).
2. [~] [x] QueryMemory por segmento (Code RX, Code R, CodeData RW). [ ] Excepciones de C++ y longjmp
   (necesitan continuaciones en los landing pads y en los puntos de retorno: diseño pendiente).
3. [~] [x] Barrera después de STLR. [ ] Accesos a memoria del juego como atómicos relajados antes de
   optimizar registros.
4. [ ] Llamadas de cola garantizadas (musttail) para que la pila del host no crezca.
5. [ ] Escala: probar con un main de 60–100 MB (memoria de Python, tiempo de clang).
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
5. [ ] `hid` (evento de estilos), `time` (zonas horarias, snapshots), `apm`, `set` (región, mapa).
6. [ ] Audio con las revisiones del SDK (5–15), efectos, IAudioDevice, `audout` Stop, `hwopus`.
7. [ ] Herramienta que lista los servicios que nombra el juego antes del primer arranque.
   Prueba: guest/libnx/cuentas (guest/esperado/cuentas.txt). NX_TRAZA_IPC=1 muestra cada comando.

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
8. [ ] Render targets en capas (gl_Layer), limpieza por capa.
9. [ ] Transform feedback.
10. [ ] Raster y samplers: ([x] logic op, [x] alpha test), blend avanzado, depth clamp, viewport swizzle, cubos sin
    costuras, reducción min/max, nivel base/máximo y LOD mínimo, coordenadas sin normalizar,
    texturas multisample y PIXLD, lectura de stencil.
11. [ ] Origen abajo-izquierda completo (gl_FragCoord.y, scissor, SR_Y_DIRECTION).
12. [ ] Etapas y registros: geometría *passthrough*, VertexA, primitive ID, máscaras de hilos.
13. [ ] Estado por canal (cada cola de NVN es un canal), espacio de direcciones con sparse y Remap,
    páginas sin mapear que leen 0.
14. [~] Presentación: [x] intervalo de swap (30 fps), [x] recorte y espejos del QueueBuffer.
    Falta: nvdec/vic para videos; presentar directo desde la textura de la GPU (hoy se deshace el
    block linear en la CPU en cada cuadro).

## CPU y recompilador

1. [ ] Tablas de salto con formas que hoy no se reconocen (base lejos, índice copiado antes, máscaras
   de IL2CPP, cmp de 64 bits...). Saltos indirectos fallidos a un archivo de pistas para recompilar,
   en vez de terminar.
2. [ ] Excepciones de C++ y longjmp; QueryMemory por segmento (Code RX / Code / CodeData) para el
   desenrollador del SDK.
3. [ ] Accesos a memoria del juego como atómicos relajados (antes de optimizar registros) y una
   barrera después de los stores con release.
4. [ ] Llamadas de cola garantizadas (musttail) para que la pila del host no crezca.
5. [ ] Escala: probar con un main de 60–100 MB (memoria de Python, tiempo de clang).
6. [ ] NRO cargados con nn::ro (ldr:ro), ZBIC, relocaciones TLS.

## Kernel

1. [ ] ID del hilo en TLS+0x110 (SDK 22).
2. [ ] GetInfo 16/17 (tamaño de recursos del sistema, del NPDM), 18 (program ID), 20 (contexto de
   excepción), 25 (ticks del hilo).
3. [ ] SetHeapSize: devolver "sin memoria" (0xD001) por encima del límite.
4. [ ] WaitForAddress tipo 3 (64 bits), SignalToAddress tipo 2 con cuenta ≤ 0.
5. [ ] SetThreadActivity / GetThreadContext3 (recolectores de basura de Unity y .NET).
6. [ ] Número de procesador dentro de la máscara del hilo; prioridad y núcleo del hilo principal
   desde el NPDM.
7. [ ] SVC que faltan: 0x30/0x31, 0x2A/0x2B, 0x22, 0x24.

## Servicios

1. [ ] `acc:u0` (usuario, perfil) y el parámetro de lanzamiento con el usuario preseleccionado.
2. [ ] Partidas guardadas en `fsp-srv` (22, 32, 51, 61, caché, dispositivo, temporal, 202, 203,
   Commit); `am` 25/26/27.
3. [ ] Servicios que responden "sin conexión" o "aceptado": aoc, pl (fuentes compartidas), pctl,
   friends, prepo, nifm, csrng, bcat, olsc, nfp, caps, ldn, npns, mii, ngc, irs, bsd/sfdnsres, ssl.
4. [ ] `am`: applets que cierran al momento, notificaciones, estadísticas, idioma desde NX_IDIOMA.
5. [ ] `hid` (evento de estilos), `time` (zonas horarias, snapshots), `apm`, `set` (región, mapa).
6. [ ] Audio con las revisiones del SDK (5–15), efectos, IAudioDevice, `audout` Stop, `hwopus`.
7. [ ] Herramienta que lista los servicios que nombra el juego antes del primer arranque.

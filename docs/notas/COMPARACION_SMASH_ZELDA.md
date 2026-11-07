# Comparación local: Smash y Tears of the Kingdom

Revisado el 3 de noviembre de 2026. No se modificó la instalación de Smash.

## Ejecutables y arranque

Ambos juegos son ejecutables ELF x86-64 nativos de Linux. El de Smash es
`../ssbu-recomp/build-linux/work/ssbu_native`; el de Zelda está en
`~/.local/share/nx2linux/juegos/0100f2c0115b6000/juego`.

El acceso de Smash ejecuta `~/.local/share/ssbu-recomp/launch.sh`, que llama a
`scripts/run.py --linux`. Este prepara las rutas de RomFS, los datos de usuario
y las opciones gráficas. El acceso de Zelda usa `app/nx2linux.sh --jugar` y
`motor.jugar`. Cambiar el acceso no corrige los errores de comandos gráficos.

## Diferencia gráfica comprobada en el código

Smash sustituye funciones de NVN por implementaciones del host:

- `../ssbu-recomp/runtime/nvn_stub.c`: entrega funciones nativas por nombre.
- `../ssbu-recomp/runtime/nvn_gl.c`: registra comandos propios. `cmd_end` conserva
  la lista; `cmd_call` copia los handles; `queue_submit` copia los handles antes
  de entregarlos al hilo gráfico. El archivo sirve también como frontend para
  Vulkan pese a su nombre.
- `../ssbu-recomp/runtime/nvn_vk.c` y `nvn_vk_draw.c`: ejecutan y presentan con Vulkan.

Zelda ejecuta su NVN traducido y genera comandos Maxwell. `runtime/nxgpu.c`
interpreta esas entradas; `runtime/nxgl.c` las dibuja con OpenGL y escribe los
resultados a la memoria que presenta `nxsvc_vi.c`.

El registro existente de Smash confirma RTX4060, Vulkan y cuadros presentados.
El registro nativo de Zelda confirma la misma GPU con OpenGL, pero contiene
cabeceras de comandos inválidas. La captura de Zelda continúa negra.

## Pruebas realizadas para acotar el fallo

- BindBlendState: cuatro comparaciones C/ARM originales coinciden.
- EndRecording: cuatro comparaciones de objeto y metadatos coinciden.
- CallCommands: ocho comparaciones de objeto y bytes modificados coinciden.
- SetViewport: cuatro comparaciones; los 36 bytes generados coinciden.
- Captura de GPFIFO: se observaron entradas de 170 palabras que terminan con un
  paquete al que falta un argumento. La entrada siguiente se interpreta mal.
- Auditoría temporal de memoria grabada: hay diferencias antes del envío, pero
  todavía debe distinguirse modificación válida, remapeo y reutilización.
  No se ha demostrado que sean la causa ni se aplica una restauración forzada.
- En otras ejecuciones aparecieron destinos omitidos 0x86f543c y 0x86f5448.
  Se verificó su familia en ARM64 y se añadieron al candidato, junto con
  0x86f5454/0x86f5460. No se sustituyó el juego instalado con este candidato.

La construcción nativa está completa. La corrección del arranque y de la imagen
sigue pendiente; las pruebas de captura no acreditan menú o partida funcional.

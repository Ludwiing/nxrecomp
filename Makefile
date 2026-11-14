# guest (ARM64, formato Switch) -> NRO -> C -> ejecutable nativo de Linux x86_64
CLANG ?= clang
B := build
PROGS := hola neon vm multi
# programas que hablan con el kernel o los servicios: su salida se compara con guest/esperado/
PROGS_ESPERADO := hilos servicios retorno_br romper
# Homebrew hechos con libnx (devkitPro): se compilan con su toolchain si está instalado.
DEVKITPRO ?= /opt/devkitpro
LIBNX_PROGS := $(if $(wildcard $(DEVKITPRO)/libnx),basico sistema kernel cuentas)
# Ejemplos oficiales de libnx con pantalla: se compara la huella del cuadro 10 (sin ventana).
LIBNX_PANTALLA := $(if $(wildcard $(DEVKITPRO)/libnx),hola_mundo simplegfx)
# deko3d por la GPU: el color de limpieza y el triángulo con sus colores interpolados.
LIBNX_GPU := $(if $(wildcard $(DEVKITPRO)/libnx),deko_basic deko_console deko_imagen deko_gpu)
PIXELES_deko_basic := 0 0 32,75,122 1270 10 32,75,122 640 360 127,64,64 640 20 248,3,4 100 700 7,231,17 1180 700 7,16,232
# deko_console: texto con una textura de fuente (arreglo 2D subido con el motor de copia), 3600 instancias.
# deko_imagen: un shader de cómputo pinta (x, y, x xor y) en una imagen que después se muestra.
# deko_gpu: draws indirectos, alpha test, logic op XOR y cuatro cuadros verdes si los contadores de
# muestras, las marcas de tiempo (614,4 MHz), los semáforos (acquire, reducción) y un pool sparse
# armado con Remap (dos trozos y un hueco que lee ceros) dieron lo esperado;
# abajo, un draw con el rasterizador apagado (no se ve), un atributo sin escribir (= 0,0,0,1) y un
# render target leído como textura con el swizzle cambiado y como arreglo (vistas de OpenGL), y las dos
# capas de un arreglo dibujadas en un solo draw con gl_Layer (naranja y violeta), y una textura que la CPU
# reescribe antes de cada cuadro sin avisar (tiene que verse el color del cuadro 5).
PIXELES_deko_gpu := 150 520 255,255,255 400 520 255,0,0 650 520 0,255,0 850 520 0,0,0 950 520 0,0,255 1150 520 0,255,255 100 270 0,255,0 250 270 0,255,0 400 270 0,255,0 620 270 0,0,0 550 270 0,255,0 150 110 0,0,0 400 110 255,255,128 650 110 153,102,51 900 110 51,102,153 1100 110 255,153,0 1200 110 153,0,255 700 270 250,5,0
PIXELES_deko_imagen := 100 50 100,50,86 159 159 159,159,0 37 140 37,140,169 300 300 0,0,0
PIXELES_deko_console := 0 0 0,0,0 231 21 191,191,191 56 98 0,127,0 133 357 255,0,0 357 357 64,0,0 168 385 255,255,0 133 406 0,0,255
# deko_examples (los 9 ejemplos oficiales de deko3d, elegidos desde su menú con NX_PULSAR). Varios
# están animados: se comprueba la fracción de píxeles dibujados y su color medio. Necesita glm
# (switch-glm, o sus cabeceras en ~/src/glm).
DEKO_EJEMPLOS := $(if $(and $(wildcard $(DEVKITPRO)/libnx),$(wildcard $(DEVKITPRO)/portlibs/switch/include/glm $(HOME)/src/glm/glm)),1 2 3 4 5 6 7 8 9)
EJEMPLO_1 := 0.95 1.0 31,92,32 25
EJEMPLO_2 := 0.45 0.55 84,84,84 20
EJEMPLO_3 := 0.10 0.35
EJEMPLO_4 := 0.10 0.35 141,124,109 40
EJEMPLO_5 := 0.03 0.09
EJEMPLO_6 := 0.10 0.35
EJEMPLO_7 := 0.08 0.30 114,97,42 35
EJEMPLO_8 := 0.08 0.30 115,99,43 35
EJEMPLO_9 := 0.005 0.05

# Shaders de prueba: GLSL → uam (el compilador de deko3d) → código Maxwell → nxshader → GLSL, y la
# imagen tiene que ser la misma que la del GLSL original en la GPU de esta máquina.
UAM := $(DEVKITPRO)/tools/bin/uam
SHADERS := $(if $(wildcard $(UAM)),$(patsubst tests/shaders/%.frag,%,$(wildcard tests/shaders/*.frag)))
SHADERS_COMP := $(if $(wildcard $(UAM)),$(patsubst tests/shaders/%.comp,%,$(wildcard tests/shaders/*.comp)))
# Mandos: un guion aprieta botones en cuadros fijos y se compara lo que el programa vio.
LIBNX_MANDOS := $(if $(wildcard $(DEVKITPRO)/libnx),mandos)
PULSAR := 10:a,20:b+x,30:up,40:zl+r,50:minus,60:plus
# Audio: el ejemplo oficial playtone toca tres notas (A, B, X); se compara el WAV grabado.
LIBNX_AUDIO := $(if $(wildcard $(DEVKITPRO)/libnx),playtone)
# El renderizador (audren): el ejemplo audren-simple tiene que sacar data/sample.bin intacto.
LIBNX_AUDREN := $(if $(wildcard $(DEVKITPRO)/libnx),audren_simple)
# Las revisiones del SDK de Nintendo (REV11 y REV15) por IPC directo: la mezcla vuelve por un efecto AUX.
LIBNX_AUDREN_REV := $(if $(wildcard $(DEVKITPRO)/libnx),audren_rev)


OPT ?= -O2
# -ffp-contract=off en las dos versiones: ningún compilador fusiona a*b+c por su cuenta.
GUEST_CFLAGS := --target=aarch64-none-elf $(OPT) -ffreestanding -fno-builtin -nostdlib \
	-fno-pic -fno-stack-protector -ffp-contract=off -fno-math-errno \
	-fno-asynchronous-unwind-tables -fno-unwind-tables -Wall
CFLAGS_hola := -mgeneral-regs-only
CFLAGS_hilos := -mgeneral-regs-only
CFLAGS_servicios := -mgeneral-regs-only
CFLAGS_romper_abort := -mgeneral-regs-only
CFLAGS_romper_ilegible := -mgeneral-regs-only
CFLAGS_romper := -mgeneral-regs-only
CFLAGS_neon := -O3
SDL_CFLAGS := $(shell pkg-config --cflags sdl3)
SDL_LIBS := $(shell pkg-config --libs sdl3)
HOST_CFLAGS := -O2 -march=x86-64-v3 -Iruntime $(SDL_CFLAGS) -Wall -Wno-unused-label -Wno-unused-function \
	-Wno-unused-value -Wno-unused-variable -Wno-parentheses-equality
RUNTIME := runtime/nxrt.c runtime/nxmain.c runtime/nxkern.c runtime/nxipc.c runtime/nxservices.c runtime/nxsvc_am.c runtime/nxsvc_ldr.c runtime/nxsvc_hid.c runtime/nxsvc_time.c runtime/nxsvc_fs.c runtime/nxsvc_vi.c runtime/nxdisplay.c runtime/nxinput.c runtime/nxaudio.c runtime/nxsvc_audio.c runtime/nxsvc_audren.c runtime/nxaudren.c runtime/nxsvc_acc.c runtime/nxsvc_otros.c runtime/nxsvc_opus.c runtime/nxgpu.c runtime/nxgl.c runtime/nxshader.c runtime/nxastc.c runtime/nxnvn.c runtime/nxhle.c runtime/nxcodec.c runtime/nxguest.c runtime/nxhle_event.c runtime/nxcodec_hle.c runtime/nxfont.c
RUNTIME_H := $(wildcard runtime/*.h)
RECOMP := $(wildcard recomp/*.py)

all: $(PROGS:%=$(B)/%) $(PROGS:%=$(B)/%_ref) $(PROGS_ESPERADO:%=$(B)/%) $(B)/romfs_prueba.img $(LIBNX_PROGS:%=$(B)/libnx_%) $(LIBNX_PANTALLA:%=$(B)/libnx_%) $(LIBNX_GPU:%=$(B)/libnx_%) $(B)/astc_test $(if $(LIBNX_GPU),$(B)/libnx_deko_astc) $(if $(DEKO_EJEMPLOS),$(B)/libnx_deko_examples) $(if $(SHADERS),$(B)/shader_test $(SHADERS:%=$(B)/shaders/%.dksh) $(SHADERS_COMP:%=$(B)/shaders/%.dksh)) $(LIBNX_MANDOS:%=$(B)/libnx_%) $(LIBNX_AUDIO:%=$(B)/libnx_%) $(LIBNX_AUDREN:%=$(B)/libnx_%) $(LIBNX_AUDREN_REV:%=$(B)/libnx_%)

$(B)/romfs_prueba.img: | $(B)
	python3 -c "import sys; sys.stdout.buffer.write(bytes((i * 31 + (i >> 8)) & 255 for i in range(65536)))" > $@

$(B)/start.o: guest/start.S | $(B)
	$(CLANG) $(GUEST_CFLAGS) -c $< -o $@

$(B)/%.o: guest/%.c guest/mini.h | $(B)
	$(CLANG) $(GUEST_CFLAGS) $(CFLAGS_$*) -c $< -o $@

$(B)/%.elf: $(B)/start.o $(B)/%.o guest/link.ld
	ld.lld -T guest/link.ld -static -nostdlib $(B)/start.o $(B)/$*.o -o $@

$(B)/%.nro: $(B)/%.elf tools/mknro.py
	python3 tools/mknro.py $< $@

$(B)/%_rec/programa.c: $(B)/%.nro $(RECOMP)
	python3 recomp/nxrecomp.py $< -o $(B)/$*_rec

$(B)/%: $(B)/%_rec/programa.c $(RUNTIME) $(RUNTIME_H)
	$(CLANG) $(HOST_CFLAGS) -I$(B)/$*_rec $(B)/$*_rec/*.c $(RUNTIME) -lm -lpthread $(SDL_LIBS) -lEGL -lOpenGL -lopus -lvpx -o $@

# El mismo programa compilado directo para x86_64: su salida es la referencia.
$(B)/%_ref: guest/%.c guest/mini.h | $(B)
	$(CLANG) -O2 -ffp-contract=off -DNATIVE_REF $< -o $@

# Par de módulos NSO enlazados entre sí (main importa del sdk), como el ExeFS de un juego.
MOD_CFLAGS := --target=aarch64-none-elf -O2 -fPIC -ffreestanding -fno-builtin -nostdlib -fno-stack-protector \
	-ffp-contract=off -fno-math-errno -fno-asynchronous-unwind-tables -fno-unwind-tables -Wall
M := $(B)/multi_mod

$(M)/%.o: guest/multi/%.c guest/mini.h | $(M)
	$(CLANG) $(MOD_CFLAGS) -c $< -o $@

$(M)/modstart.o: guest/multi/modstart.S | $(M)
	$(CLANG) $(MOD_CFLAGS) -c $< -o $@

$(M)/%.so: $(M)/%.o $(M)/modstart.o guest/multi/module.ld
	ld.lld -shared -T guest/multi/module.ld --hash-style=sysv -z norelro -soname $* $(M)/modstart.o $< -o $@

$(M)/exefs/main.npdm: tools/mknso.py | $(M)
	mkdir -p $(M)/exefs
	python3 tools/mknso.py --npdm multi $@

$(M)/exefs/%: $(M)/%.so tools/mknso.py recomp/nso.py
	mkdir -p $(M)/exefs
	python3 tools/mknso.py $< $@

$(B)/multi_rec/programa.c: $(M)/exefs/main $(M)/exefs/sdk $(M)/exefs/main.npdm $(RECOMP)
	python3 recomp/nxrecomp.py $(M)/exefs -o $(B)/multi_rec

$(B)/multi_ref: guest/multi/main.c guest/multi/sdk.c guest/multi/ref.c guest/mini.h | $(B)
	$(CLANG) -O2 -ffp-contract=off -DNATIVE_REF guest/multi/main.c guest/multi/sdk.c guest/multi/ref.c -o $@

$(M):
	mkdir -p $@

# ASTC: el decodificador contra el de Mesa (llvmpipe), con bloques aleatorios de los 14 tamaños.
$(B)/astc_test: tests/astc_test.c runtime/nxastc.c runtime/nxastc.h | $(B)
	$(CLANG) -O2 -Iruntime tests/astc_test.c runtime/nxastc.c -lEGL -lOpenGL -o $@

$(B)/shader_test: tests/shader_test.c runtime/nxshader.c runtime/nxshader.h runtime/mxisa.h | $(B)
	$(CLANG) -O1 -Iruntime tests/shader_test.c runtime/nxshader.c -lEGL -lOpenGL -o $@

$(B)/shaders/%.dksh: tests/shaders/%.frag
	@mkdir -p $(B)/shaders
	$(UAM) -s frag -o $@ $< 2>/dev/null

$(B)/shaders/%.dksh: tests/shaders/%.comp
	@mkdir -p $(B)/shaders
	$(UAM) -s comp -o $@ $< 2>/dev/null

$(B)/libnx_deko_examples: guest/libnx/deko_examples/source/main.cpp $(RECOMP) $(RUNTIME) $(RUNTIME_H) | $(B)
	DEVKITPRO=$(DEVKITPRO) $(MAKE) -s -C guest/libnx/deko_examples >/dev/null
	python3 recomp/nxrecomp.py guest/libnx/deko_examples/deko_examples.nro -o $(B)/libnx_deko_examples_rec >/dev/null
	python3 tools/compilar.py $(B)/libnx_deko_examples_rec -o $@ >/dev/null

$(B)/libnx_%: guest/libnx/%/source/main.c $(RECOMP) $(RUNTIME) $(RUNTIME_H) | $(B)
	DEVKITPRO=$(DEVKITPRO) $(MAKE) -s -C guest/libnx/$* >/dev/null
	python3 recomp/nxrecomp.py guest/libnx/$*/$*.nro -o $(B)/libnx_$*_rec >/dev/null
	python3 tools/compilar.py $(B)/libnx_$*_rec -o $@ >/dev/null

$(B)/ipc_tipc_test: tests/ipc_tipc_test.c runtime/nxipc.c $(RUNTIME_H) | $(B)
	$(CLANG) -O2 -march=x86-64-v3 -Iruntime tests/ipc_tipc_test.c runtime/nxipc.c -o $@

$(B)/applet_startup_test: tests/applet_startup_test.c runtime/nxsvc_am.c $(RUNTIME_H) | $(B)
	$(CLANG) -O2 -march=x86-64-v3 -Iruntime -ffunction-sections -fdata-sections -Wl,--gc-sections $< -o $@

$(B)/ldr_ro_test: tests/ldr_ro_test.c runtime/nxsvc_ldr.c $(RUNTIME_H) | $(B)
	$(CLANG) -O2 -march=x86-64-v3 -Iruntime tests/ldr_ro_test.c runtime/nxsvc_ldr.c -o $@

$(B)/native_window_test: tests/native_window_test.c runtime/nxsvc_vi.c $(RUNTIME_H) | $(B)
	$(CLANG) -O2 -march=x86-64-v3 -Iruntime -ffunction-sections -fdata-sections -Wl,--gc-sections $< -o $@

$(B)/vi2_session_test: tests/vi2_session_test.c runtime/nxsvc_vi.c $(RUNTIME_H) | $(B)
	$(CLANG) -O2 -march=x86-64-v3 -Iruntime -ffunction-sections -fdata-sections -Wl,--gc-sections $< -o $@

$(B)/pl_fuentes_test: tests/pl_fuentes_test.c runtime/nx_pl.h | $(B)
	$(CLANG) -O2 -Wall -Wextra -Iruntime $< -o $@

$(B)/nxfont_work_test: tests/nxfont_work_test.c runtime/nxfont.c runtime/nxrt.h $(RUNTIME_H) | $(B)
	$(CLANG) -O2 -Wall -Wextra -Iruntime tests/nxfont_work_test.c runtime/nxfont.c -lpthread -o $@

$(B)/nvn_device_info_test: tests/nvn_device_info_test.c runtime/nvn/nvn_device_info.h | $(B)
	$(CLANG) -O2 -Iruntime/nvn $< -o $@

.PHONY: test-startup test-identidad test-codec
$(B)/nxcodec_vp9_test: tests/nxcodec_vp9_test.c runtime/nxcodec.c runtime/nxcodec.h | $(B)
	$(CLANG) -O2 -Wall -Wextra -Iruntime tests/nxcodec_vp9_test.c runtime/nxcodec.c -lvpx -o $@

$(B)/nxcodec_hle_test: tests/nxcodec_hle_test.c runtime/nxcodec_hle.c runtime/nxcodec.c runtime/nxguest.c runtime/nxhle_event.c $(RUNTIME_H) | $(B)
	$(CLANG) -O1 -Wall -Wextra -Iruntime tests/nxcodec_hle_test.c runtime/nxcodec_hle.c runtime/nxcodec.c -lvpx -lpthread -o $@

test-codec: $(B)/nxcodec_vp9_test $(B)/nxcodec_hle_test
	$(B)/nxcodec_vp9_test
	$(B)/nxcodec_hle_test

$(B)/nxkern_event_handle_test: tests/nxkern_event_handle_test.c runtime/nxkern.c $(RUNTIME_H) | $(B)
	$(CLANG) -O1 -Iruntime -ffunction-sections -fdata-sections -Wl,--gc-sections $< runtime/nxkern.c -lpthread -o $@

# Nintendo Switch Sports Resort al arrancar: chat GSV por TIPC (id, evento, TryWait, trazas de espera acotadas),
# chat:u, ThreadType del CreateThread HLE y datos del sistema por $NX_DATOS_SISTEMA
$(B)/chat_gsv_tipc_test: tests/chat_gsv_tipc_test.c runtime/nxsvc_am.c runtime/nxkern.c runtime/nxipc.c $(RUNTIME_H) | $(B)
	$(CLANG) -O1 -Iruntime -ffunction-sections -fdata-sections -Wl,--gc-sections $< runtime/nxkern.c runtime/nxipc.c -lpthread -o $@
$(B)/chat_servicio_test: tests/chat_servicio_test.c runtime/nxsvc_otros.c $(RUNTIME_H) | $(B)
	$(CLANG) -O1 -Iruntime -ffunction-sections -fdata-sections -Wl,--gc-sections $< -lpthread -o $@
$(B)/mii_servicio_test: tests/mii_servicio_test.c runtime/nxsvc_otros.c $(RUNTIME_H) | $(B)
	$(CLANG) -O1 -Iruntime -ffunction-sections -fdata-sections -Wl,--gc-sections $< -lpthread -o $@
$(B)/thread_create_stack_test: tests/thread_create_stack_test.c runtime/nxkern.c $(RUNTIME_H) | $(B)
	$(CLANG) -O1 -Iruntime -ffunction-sections -fdata-sections -Wl,--gc-sections $< runtime/nxkern.c -lpthread -o $@
$(B)/datos_sistema_test: tests/datos_sistema_test.c runtime/nxsvc_fs.c $(RUNTIME_H) | $(B)
	$(CLANG) -O1 -Iruntime -ffunction-sections -fdata-sections -Wl,--gc-sections $< -lpthread -o $@

$(B)/nxguest_test: tests/nxguest_test.c runtime/nxguest.c runtime/nxrt.h | $(B)
	$(CLANG) -O1 -Iruntime tests/nxguest_test.c runtime/nxguest.c -o $@

$(B)/nxhle_event_test: tests/nxhle_event_test.c runtime/nxhle_event.c runtime/nxguest.c $(RUNTIME_H) | $(B)
	$(CLANG) -O1 -Iruntime tests/nxhle_event_test.c runtime/nxhle_event.c runtime/nxguest.c -o $@

# Identidad de los juegos de NX2Linux (Title ID por estructura, .nup incompleto/ambiguo, nombre visible), sin juegos reales
test-identidad:
	python3 tests/identidad_juego_test.py

.PHONY: test-gpu-state
$(B)/renderer_config_test: tests/renderer_config_test.c runtime/nxmain.c $(RUNTIME_H) | $(B)
	$(CLANG) -O1 -Iruntime -ffunction-sections -fdata-sections -Wl,--gc-sections $< -o $@

$(B)/clip_depth_test: tests/clip_depth_test.c runtime/nxgl.c $(RUNTIME_H) | $(B)
	$(CLANG) -O1 -Iruntime $(SDL_CFLAGS) -ffunction-sections -fdata-sections -Wl,--gc-sections $< -lEGL -lOpenGL -lm -o $@

$(B)/gpu_submission_snapshot_test: tests/gpu_submission_snapshot_test.c runtime/nxgpu.c $(RUNTIME_H) | $(B)
	$(CLANG) -O1 -Iruntime -ffunction-sections -fdata-sections -Wl,--gc-sections $< -lpthread -lm -o $@

test-gpu-state: $(B)/clip_depth_test $(B)/gpu_submission_snapshot_test $(B)/renderer_config_test
	$(B)/clip_depth_test
	$(B)/gpu_submission_snapshot_test
	$(B)/renderer_config_test

$(B)/hid_startup_test: tests/hid_startup_test.c runtime/nxsvc_hid.c $(RUNTIME_H) | $(B)
	$(CLANG) -O2 -Iruntime $< -o $@

$(B)/hid_npad_condition_test: tests/hid_npad_condition_test.c runtime/nxsvc_hid.c $(RUNTIME_H) | $(B)
	$(CLANG) -O2 -Wall -Iruntime $< -o $@

$(B)/hid_npad_seqlock_test: tests/hid_npad_seqlock_test.c runtime/nxinput.c $(RUNTIME_H) | $(B)
	$(CLANG) -O2 -Iruntime $(SDL_CFLAGS) $< $(SDL_LIBS) -lpthread -o $@

$(B)/hid_key_cycle_test: tests/hid_key_cycle_test.c runtime/nxinput.c $(RUNTIME_H) | $(B)
	$(CLANG) -O2 -Wall -Iruntime $(SDL_CFLAGS) $< $(SDL_LIBS) -lpthread -o $@

$(B)/storage_concurrency_test: tests/storage_concurrency_test.c runtime/nxsvc_fs.c $(RUNTIME_H) | $(B)
	$(CLANG) -O2 -Iruntime -ffunction-sections -fdata-sections -Wl,--gc-sections $< -lpthread -o $@

$(B)/nv_sync_event_test: tests/nv_sync_event_test.c runtime/nxsvc_vi.c runtime/nxgpu.c $(RUNTIME_H) | $(B)
	$(CLANG) -O1 -Iruntime $(SDL_CFLAGS) -ffunction-sections -fdata-sections -Wl,--gc-sections $< runtime/nxgpu.c -lpthread -lEGL -lOpenGL -lm -o $@

test-startup: $(B)/ipc_tipc_test $(B)/applet_startup_test $(B)/ldr_ro_test $(B)/pl_fuentes_test $(B)/nxfont_work_test $(B)/native_window_test $(B)/vi2_session_test $(B)/nvn_device_info_test $(B)/hid_startup_test $(B)/hid_npad_condition_test $(B)/hid_npad_seqlock_test $(B)/hid_key_cycle_test $(B)/storage_concurrency_test $(B)/nv_sync_event_test $(B)/nxkern_event_handle_test $(B)/chat_gsv_tipc_test $(B)/chat_servicio_test $(B)/mii_servicio_test $(B)/thread_create_stack_test $(B)/datos_sistema_test
	$(B)/ipc_tipc_test
	$(B)/applet_startup_test
	$(B)/ldr_ro_test
	$(B)/pl_fuentes_test
	$(B)/nxfont_work_test
	$(B)/native_window_test
	$(B)/vi2_session_test
	$(B)/nvn_device_info_test
	$(B)/hid_startup_test
	$(B)/hid_npad_condition_test
	$(B)/hid_npad_seqlock_test
	$(B)/hid_key_cycle_test
	$(B)/storage_concurrency_test
	$(B)/nv_sync_event_test
	$(B)/nxkern_event_handle_test
	$(B)/nxguest_test
	$(B)/nxhle_event_test
	$(B)/chat_gsv_tipc_test
	$(B)/chat_servicio_test
	$(B)/mii_servicio_test
	$(B)/thread_create_stack_test
	$(B)/datos_sistema_test
	python3 tests/construccion_cancelar_test.py
	python3 tests/romfs_native_test.py
	python3 tests/dispatch_table_test.py
	python3 tests/branch_local_test.py
	python3 tests/compile_cache_test.py
	python3 tests/compile_cancel_test.py

$(B)/framegen_test: tests/framegen_test.c runtime/nvn/fg_store_spv.h runtime/nvn/fg_luma_spv.h runtime/nvn/fg_motion_spv.h runtime/nvn/fg_median_spv.h runtime/nvn/fg_interp_spv.h | $(B)
	$(CLANG) -O1 -Iruntime/nvn tests/framegen_test.c -lvulkan -lm -o $@

# Los SPIR-V de la generación de cuadros (runtime/shaders/fg_*.comp), con glslc
runtime/nvn/fg_%_spv.h: runtime/shaders/fg_%.comp tools/spv_header.py
	python3 tools/spv_header.py $<

$(B)/nvn2_program_test: tests/nvn2_program_test.c runtime/nvn/nvn2_program.h | $(B)
	$(CLANG) -O1 -Iruntime/nvn tests/nvn2_program_test.c -o $@

# Memoria de comandos y de control de los command buffers NVN: callbacks al agotarse y contadores (nvn_api_cmd.c)
.PHONY: test-nvn-cmd
$(B)/nvn_cmd_memory_test: tests/nvn_cmd_memory_test.c runtime/nvn/nvn_api_cmd.c runtime/nvn/nvn_api.h runtime/nvn/nvn_obj.h | $(B)
	$(CLANG) -O1 -Wall -Wextra -Iruntime -Iruntime/nvn -Iruntime/nvn/third_party/vma tests/nvn_cmd_memory_test.c runtime/nvn/nvn_api_cmd.c -o $@

test-nvn-cmd: $(B)/nvn_cmd_memory_test
	$(B)/nvn_cmd_memory_test 2>$(B)/nvn_cmd_memory.txt || { cat $(B)/nvn_cmd_memory.txt; exit 1; }

# Decodificador SM86 (Switch 2) en C contra el de Python; sm86_tablas.h se regenera a mano con tools/sm86_c.py
$(B)/sm86_dec_test: tests/sm86_dec_test.c runtime/nvn/sm86_dec.c runtime/nvn/sm86_dec.h runtime/nvn/sm86_tablas.h | $(B)
	$(CLANG) -O2 -march=x86-64-v3 -Wall -Wextra -Iruntime/nvn tests/sm86_dec_test.c runtime/nvn/sm86_dec.c -o $@

# Dirección del invitado -> módulo y símbolo (svcBreak, pila del invitado)
$(B)/describir_test: tests/describir_test.c runtime/nx_describir.h runtime/nxrt.h | $(B)
	$(CLANG) -O1 -Wall -Iruntime tests/describir_test.c -o $@

# La bajada SM86 -> Maxwell propia del runtime (NX_SM86_BAJADA=propia) contra armar() de Python
$(B)/sm86_propia_test: tests/sm86_propia_test.c runtime/nvn/sm86_propia.c runtime/nvn/sm86_propia.h | $(B)
	$(CLANG) -O1 -Wall -Wextra -Iruntime/nvn tests/sm86_propia_test.c runtime/nvn/sm86_propia.c -o $@

# Corre en la GPU el SPIR-V de cómputo de un programa SM86 bajado a Maxwell (tests/sm86_programa_test.py)
$(B)/sm86_programa_vk: tests/sm86_programa_vk.c | $(B)
	$(CLANG) -O1 -Wall tests/sm86_programa_vk.c -lvulkan -o $@

$(B)/gob_test: tests/gob_test.c runtime/nvn/tex_deswizzle.c runtime/nvn/tex.h | $(B)
	$(CLANG) -O1 -Iruntime -Iruntime/nvn -Iruntime/nvn/third_party/vma tests/gob_test.c runtime/nvn/tex_deswizzle.c -o $@

# El deswizzle en la GPU (runtime/shaders/deswizzle.comp) contra tex_bl_copy / nvn_bl_offset, Switch 1 y Switch 2
$(B)/deswizzle_gpu_test: tests/deswizzle_gpu_test.c runtime/nvn/deswizzle_spv.h runtime/nvn/tex_deswizzle.c runtime/nvn/tex.h | $(B)
	$(CLANG) -O1 -Iruntime -Iruntime/nvn -Iruntime/nvn/third_party/vma tests/deswizzle_gpu_test.c runtime/nvn/tex_deswizzle.c -lvulkan -o $@

$(B)/vk_vma.o: runtime/nvn/vk_vma.cpp runtime/nvn/third_party/vma/vk_mem_alloc.h | $(B)
	clang++ -c -O1 -Iruntime/nvn/third_party/vma runtime/nvn/vk_vma.cpp -o $@

$(B)/vk_pool_fisico_test: tests/vk_pool_fisico_test.c runtime/nvn/vk_phys_mem.h $(B)/vk_vma.o | $(B)
	$(CLANG) -O1 -Iruntime/nvn -Iruntime/nvn/third_party/vma tests/vk_pool_fisico_test.c $(B)/vk_vma.o -lvulkan -lstdc++ -o $@

runtime/nvn/deswizzle_spv.h: runtime/shaders/deswizzle.comp tools/spv_header.py
	python3 tools/spv_header.py $<

test: all test-startup test-identidad test-codec test-nvn-cmd $(B)/framegen_test $(B)/vk_pool_fisico_test $(B)/nvn2_program_test $(B)/gob_test $(B)/deswizzle_gpu_test $(B)/sm86_dec_test $(B)/sm86_programa_vk $(B)/sm86_propia_test $(B)/romper_abort $(B)/romper_ilegible $(B)/describir_test
	python3 tests/compilar_runtime_test.py
	@ok=1; for p in $(PROGS) $(PROGS_ESPERADO); do \
		NX_ROMFS=$(B)/romfs_prueba.img $(B)/$$p > $(B)/$$p.salida.txt; \
		if [ -f guest/esperado/$$p.txt ]; then cp guest/esperado/$$p.txt $(B)/$$p.referencia.txt; \
		else $(B)/$${p}_ref > $(B)/$$p.referencia.txt; fi; \
		if diff -u $(B)/$$p.referencia.txt $(B)/$$p.salida.txt; then echo "OK: $$p es idéntico a la referencia"; \
		else ok=0; fi; \
	done; \
	for p in $(LIBNX_PROGS); do \
		mkdir -p $(B)/sdmc; rm -rf $(B)/guardado; NX_SDMC=$(B)/sdmc NX_GUARDADO=$(B)/guardado $(B)/libnx_$$p > $(B)/$$p.salida.txt; \
		if diff -u guest/esperado/$$p.txt $(B)/$$p.salida.txt; then echo "OK: $$p (libnx) es idéntico a lo esperado"; \
		else ok=0; fi; \
	done; \
	for p in $(LIBNX_PANTALLA); do \
		NX_SIN_VENTANA=1 NX_CAPTURA=$(B)/$$p.ppm NX_CAPTURA_CUADRO=10 timeout 60 $(B)/libnx_$$p >/dev/null; \
		if [ "$$(sha256sum < $(B)/$$p.ppm | cut -d' ' -f1)" = "$$(cat guest/esperado/$$p.sha256)" ]; then \
			echo "OK: $$p (libnx, pantalla) dibuja el cuadro esperado"; else echo "MAL: $$p dibujó otro cuadro"; ok=0; fi; \
	done; \
	python3 tests/nso_test.py || ok=0; \
	python3 tests/program_boot_test.py || ok=0; \
	python3 tests/thread_hooks_test.py || ok=0; \
	python3 tests/svc_break_test.py $(B)/romper_abort $(B)/romper_ilegible || ok=0; \
	python3 tests/salidas_test.py || ok=0; \
	$(B)/describir_test || ok=0; \
	python3 tests/servicios_estrictos_test.py $(B)/servicios || ok=0; \
	python3 tests/rtld_ro_test.py || ok=0; \
	python3 tests/modulos_ro_test.py || ok=0; \
	python3 tests/plataforma_test.py || ok=0; \
	$(B)/gob_test >$(B)/gob.txt 2>&1 && echo "OK: acomodo block-linear de Switch 1 y de Switch 2 (GOB con bits 5 y 6 cambiados)" || { echo "MAL: gob_test (ver $(B)/gob.txt)"; ok=0; }; \
	python3 tests/sustituir_test.py >$(B)/sustituir.txt 2>&1 && echo "OK: sustituciones HLE (valor fijo, aviso, prefijo, varios archivos)" || { echo "MAL: sustituir_test (ver $(B)/sustituir.txt)"; ok=0; }; \
	python3 tests/wrap_sustitucion_test.py || ok=0; \
	$(B)/nvn2_program_test >/dev/null && echo "OK: secciones de control de NVN2 (Switch 2)" || { echo "MAL: nvn2_program_test"; ok=0; }; \
	python3 tests/sm86_dec_test.py $(B)/sm86_dec_test >$(B)/sm86_dec.txt 2>&1; r=$$?; \
	if [ $$r = 0 ]; then echo "OK: el decodificador SM86 en C da lo mismo que el de Python ($$(head -1 $(B)/sm86_dec.txt | cut -d: -f1))"; \
	elif [ $$r = 77 ]; then echo "(sin build/sm86_spec.json no se prueba el decodificador SM86 en C)"; else echo "MAL: decodificador SM86 en C (ver $(B)/sm86_dec.txt)"; ok=0; fi; \
	python3 tests/sm86_programa_test.py >$(B)/sm86_programa.txt 2>&1; r=$$?; \
	if [ $$r = 0 ]; then grep -h "^OK\|^(sin" $(B)/sm86_programa.txt; \
	elif [ $$r = 77 ]; then echo "(sin totk_xlate o sin CUDA no se prueba la bajada de programas SM86)"; else echo "MAL: bajada de programas SM86 (ver $(B)/sm86_programa.txt)"; ok=0; fi; \
	if [ ! -f build/sm86_spec.json ]; then echo "(sin build/sm86_spec.json no se prueba la bajada de VOTE, FSET y PLOP3)"; \
	elif python3 tools/sm86_baja_predicados.py probar 1500 >$(B)/sm86_predicados.txt 2>&1; then echo "OK: bajada de VOTE, FSET y PLOP3 ejecutada contra SM86 ($$(head -1 $(B)/sm86_predicados.txt))"; \
	else echo "MAL: bajada de VOTE, FSET y PLOP3 (ver $(B)/sm86_predicados.txt)"; ok=0; fi; \
	if [ ! -f build/sm86_spec.json ]; then echo "(sin build/sm86_spec.json no se prueba la bajada de PRMT)"; \
	elif python3 tools/sm86_baja_prmt.py probar 1500 >$(B)/sm86_prmt.txt 2>&1; then echo "OK: bajada de PRMT con BFE/BFI ejecutada contra SM86 ($$(head -1 $(B)/sm86_prmt.txt))"; \
	else echo "MAL: bajada de PRMT (ver $(B)/sm86_prmt.txt)"; ok=0; fi; \
	if [ ! -f build/sm86_spec.json ]; then echo "(sin build/sm86_spec.json no se prueba la bajada de medio float)"; \
	elif python3 tools/sm86_baja_medios.py probar 800 >$(B)/sm86_medios.txt 2>&1; then echo "OK: bajada de HADD2/HMUL2/HFMA2/HSETP2/HSET2 ejecutada contra SM86 ($$(head -1 $(B)/sm86_medios.txt))"; \
	else echo "MAL: bajada de medio float (ver $(B)/sm86_medios.txt)"; ok=0; fi; \
	if [ ! -f build/sm86_spec.json ] || ! python3 -c "import sys; sys.path.insert(0, 'tools'); import sm86_oraculo; sys.exit(0 if sm86_oraculo.disponible() else 1)"; then echo "(sin nvdisasm no se compara la lectura de medio float con el oráculo)"; \
	elif python3 tools/sm86_baja_medios.py oraculo 100 >$(B)/sm86_medios_oraculo.txt 2>&1; then echo "OK: lectura de operandos de medio float igual a nvdisasm ($$(head -1 $(B)/sm86_medios_oraculo.txt))"; \
	else echo "MAL: lectura de medio float distinta de nvdisasm (ver $(B)/sm86_medios_oraculo.txt)"; ok=0; fi; \
	if python3 -B tools/sm86_baja_suld.py probar >$(B)/sm86_suld.txt 2>&1; then echo "OK: bajada SM86 SULD.P 2D ($$(head -1 $(B)/sm86_suld.txt))"; \
	else echo "MAL: bajada SM86 SULD.P 2D (ver $(B)/sm86_suld.txt)"; ok=0; fi; \
	python3 tests/sm86_propia_test.py $(B)/sm86_propia_test >$(B)/sm86_propia.txt 2>&1; r=$$?; \
	if [ $$r = 0 ]; then grep -h "^OK" $(B)/sm86_propia.txt; \
	elif [ $$r = 77 ]; then echo "(sin build/sm86_spec.json no se prueba la bajada SM86 propia del runtime)"; else echo "MAL: bajada SM86 propia del runtime (ver $(B)/sm86_propia.txt)"; ok=0; fi; \
	python3 tests/app_entradas_test.py 2>/dev/null || ok=0; \
	$(B)/astc_test 20000 >/dev/null 2>$(B)/astc.err; r=$$?; \
	if [ $$r = 0 ]; then echo "OK: el decodificador ASTC da lo mismo que Mesa en 560 mil bloques (14 tamaños, lineal y sRGB)"; \
	elif [ $$r = 77 ]; then echo "(sin el llvmpipe de Mesa no se prueba ASTC)"; else echo "MAL: ASTC distinto de Mesa"; ok=0; fi; \
	$(B)/framegen_test >$(B)/framegen.txt 2>&1; r=$$?; \
	if [ $$r = 0 ]; then echo "OK: la generación de cuadros pone el objeto a mitad de camino (vectores exactos, error < 1/2 de la mezcla)"; \
	elif [ $$r = 77 ]; then echo "(sin Vulkan no se prueba la generación de cuadros)"; else echo "MAL: generación de cuadros (ver $(B)/framegen.txt)"; ok=0; fi; \
	$(B)/deswizzle_gpu_test >$(B)/deswizzle_gpu.txt 2>&1; r=$$?; \
	if [ $$r = 0 ]; then echo "OK: el deswizzle en la GPU da lo mismo que el del procesador byte a byte (Switch 1 y Switch 2, 400 geometrías)"; \
	elif [ $$r = 77 ]; then echo "(sin Vulkan no se prueba el deswizzle en la GPU)"; else echo "MAL: deswizzle en la GPU (ver $(B)/deswizzle_gpu.txt)"; ok=0; fi; \
	$(B)/vk_pool_fisico_test >$(B)/vk_pool_fisico.txt 2>&1; r=$$?; \
	if [ $$r = 0 ]; then echo "OK: un pool físico más grande que la VRAM libre cae en memoria de sistema en vez de rechazarse"; \
	elif [ $$r = 77 ]; then echo "(sin Vulkan o sin memoria de sistema aparte no se prueba el pool físico)"; else echo "MAL: pool físico (ver $(B)/vk_pool_fisico.txt)"; ok=0; fi; \
	if [ -n "$(LIBNX_GPU)" ]; then \
		NX_SIN_VENTANA=1 NX_CAPTURA=$(B)/deko_astc.ppm NX_CAPTURA_CUADRO=3 timeout 60 $(B)/libnx_deko_astc >/dev/null 2>&1; \
		$(B)/astc_test --captura guest/libnx/deko_astc/romfs $(B)/deko_astc.ppm 2>>$(B)/astc.err; r=$$?; \
		if [ $$r = 0 ]; then echo "OK: deko_astc dibuja sus texturas ASTC como las decodifica Mesa"; \
		elif [ $$r != 77 ]; then echo "MAL: deko_astc dibujó otras texturas"; ok=0; fi; \
	fi; \
	if [ -n "$(SHADERS)" ] && python3 -c "import numpy" 2>/dev/null; then \
		if python3 tests/half_test.py $(B)/shader_test $(B)/shaders/basico.dksh >$(B)/half.txt 2>&1; then \
			echo "OK: $$(tail -1 $(B)/half.txt) (ensamblados a mano, contra numpy)"; \
		else cat $(B)/half.txt; echo "MAL: media precisión"; ok=0; fi; \
	fi; \
	if [ -n "$(SHADERS_COMP)" ]; then \
		if $(B)/shader_test --comparar-comp $(foreach s,$(SHADERS_COMP),tests/shaders/$(s).comp $(B)/shaders/$(s).dksh); then \
			echo "OK: los $(words $(SHADERS_COMP)) shaders de cómputo traducidos escriben lo mismo que su GLSL original"; \
		else echo "MAL: algún shader de cómputo traducido escribe otra cosa"; ok=0; fi; \
	fi; \
	$(foreach n,$(DEKO_EJEMPLOS),tools/deko_ejemplo.sh $(n) $(B)/deko_ejemplo$(n).ppm >/dev/null 2>&1; \
		if python3 tools/imagen.py $(B)/deko_ejemplo$(n).ppm $(EJEMPLO_$(n)); then \
			echo "OK: deko_examples $(n) dibujó lo esperado"; else echo "MAL: deko_examples $(n) dibujó otra cosa"; ok=0; fi;) \
	if [ -n "$(SHADERS)" ]; then \
		if $(B)/shader_test --comparar $(foreach s,$(SHADERS),tests/shaders/$(s).frag $(B)/shaders/$(s).dksh); then \
			echo "OK: los $(words $(SHADERS)) shaders de prueba traducidos dibujan lo mismo que su GLSL original"; \
		else echo "MAL: algún shader traducido dibuja otra cosa"; ok=0; fi; \
	fi; \
	$(foreach p,$(LIBNX_GPU),NX_SIN_VENTANA=1 NX_CAPTURA=$(B)/$(p).ppm NX_CAPTURA_CUADRO=5 timeout 60 $(B)/libnx_$(p) >/dev/null; \
		if python3 tools/pixel.py $(B)/$(p).ppm $(PIXELES_$(p)); then \
			echo "OK: $(p) (deko3d, GPU) dibujó lo esperado"; else echo "MAL: $(p) dibujó otra cosa"; ok=0; fi;) \
	for p in $(LIBNX_MANDOS); do \
		NX_SIN_VENTANA=1 NX_PULSAR="$(PULSAR)" timeout 60 $(B)/libnx_$$p > $(B)/$$p.salida.txt; \
		if diff -u guest/esperado/$$p.txt $(B)/$$p.salida.txt; then echo "OK: $$p (libnx, mandos) vio los botones esperados"; \
		else ok=0; fi; \
	done; \
	for p in $(LIBNX_AUDIO); do \
		rm -f $(B)/$$p.wav; NX_SIN_VENTANA=1 NX_SIN_AUDIO=1 NX_AUDIO_WAV=$(B)/$$p.wav NX_PULSAR="10:a,20:b,30:x,40:plus" \
			timeout 60 $(B)/libnx_$$p >/dev/null; \
		if [ "$$(sha256sum < $(B)/$$p.wav | cut -d' ' -f1)" = "$$(cat guest/esperado/$$p.wav.sha256)" ]; then \
			echo "OK: $$p (libnx, audio) tocó el sonido esperado"; else echo "MAL: $$p tocó otro sonido"; ok=0; fi; \
	done; \
	for p in $(LIBNX_AUDREN); do \
		rm -f $(B)/$$p.wav; NX_SIN_VENTANA=1 NX_SIN_AUDIO=1 NX_AUDIO_WAV=$(B)/$$p.wav NX_PULSAR="10:a,140:plus" \
			timeout 60 $(B)/libnx_$$p >/dev/null; \
		if python3 tests/compara_audio.py $(B)/$$p.wav guest/libnx/$$p/data/sample.bin >/dev/null; then \
			echo "OK: $$p (libnx, audren) mezcló el sonido intacto"; else echo "MAL: $$p mezcló otro sonido"; ok=0; fi; \
	done; \
	for p in $(LIBNX_AUDREN_REV); do \
		NX_SIN_VENTANA=1 NX_SIN_AUDIO=1 timeout 60 $(B)/libnx_$$p > $(B)/$$p.salida.txt; \
		if diff -u guest/esperado/$$p.txt $(B)/$$p.salida.txt; then echo "OK: $$p (libnx, audren REV11 y REV15) mezcló lo esperado"; \
		else ok=0; fi; \
	done; test $$ok = 1

$(B):
	mkdir -p $@

clean:
	rm -rf $(B)

.PRECIOUS: $(M)/%.o $(M)/%.so $(B)/%.o $(B)/%.elf $(B)/%.nro $(B)/%_rec/programa.c
# ZBIC (compresión de NSO del sistema 22.0.0+ y de Switch 2): paquete zbic de kinnay (GPLv2, de
# Atmosphère) en .pydeps/, que recomp/nso.py agrega solo a sys.path; sirve al Python del sistema y al de .venv.
pydeps:
	test -x $(B)/pip-venv/bin/pip || python3 -m venv $(B)/pip-venv
	$(B)/pip-venv/bin/pip install --target .pydeps zbic==1.0.1.post1

.PHONY: all test clean pydeps

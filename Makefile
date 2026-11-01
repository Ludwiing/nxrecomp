# guest (ARM64, formato Switch) -> NRO -> C -> ejecutable nativo de Linux x86_64
CLANG ?= clang
B := build
PROGS := hola neon vm multi
# programas que hablan con el kernel o los servicios: su salida se compara con guest/esperado/
PROGS_ESPERADO := hilos servicios
# Homebrew hechos con libnx (devkitPro): se compilan con su toolchain si está instalado.
DEVKITPRO ?= /opt/devkitpro
LIBNX_PROGS := $(if $(wildcard $(DEVKITPRO)/libnx),basico sistema)
# Ejemplos oficiales de libnx con pantalla: se compara la huella del cuadro 10 (sin ventana).
LIBNX_PANTALLA := $(if $(wildcard $(DEVKITPRO)/libnx),hola_mundo simplegfx)
# deko3d por la GPU: el color de limpieza y el triángulo con sus colores interpolados.
LIBNX_GPU := $(if $(wildcard $(DEVKITPRO)/libnx),deko_basic deko_console)
PIXELES_deko_basic := 0 0 32,75,122 1270 10 32,75,122 640 360 127,64,64 640 20 248,3,4 100 700 7,231,17 1180 700 7,16,232
# deko_console: texto con una textura de fuente (arreglo 2D subido con el motor de copia), 3600 instancias.
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


OPT ?= -O2
# -ffp-contract=off en las dos versiones: ningún compilador fusiona a*b+c por su cuenta.
GUEST_CFLAGS := --target=aarch64-none-elf $(OPT) -ffreestanding -fno-builtin -nostdlib \
	-fno-pic -fno-stack-protector -ffp-contract=off -fno-math-errno \
	-fno-asynchronous-unwind-tables -fno-unwind-tables -Wall
CFLAGS_hola := -mgeneral-regs-only
CFLAGS_hilos := -mgeneral-regs-only
CFLAGS_servicios := -mgeneral-regs-only
CFLAGS_neon := -O3
SDL_CFLAGS := $(shell pkg-config --cflags sdl3)
SDL_LIBS := $(shell pkg-config --libs sdl3)
HOST_CFLAGS := -O2 -march=x86-64-v3 -Iruntime $(SDL_CFLAGS) -Wall -Wno-unused-label -Wno-unused-function \
	-Wno-unused-value -Wno-unused-variable -Wno-parentheses-equality
RUNTIME := runtime/nxrt.c runtime/nxmain.c runtime/nxkern.c runtime/nxipc.c runtime/nxservices.c runtime/nxsvc_am.c runtime/nxsvc_hid.c runtime/nxsvc_time.c runtime/nxsvc_fs.c runtime/nxsvc_vi.c runtime/nxdisplay.c runtime/nxinput.c runtime/nxaudio.c runtime/nxsvc_audio.c runtime/nxsvc_audren.c runtime/nxgpu.c runtime/nxgl.c runtime/nxshader.c runtime/nxastc.c
RUNTIME_H := $(wildcard runtime/*.h)
RECOMP := $(wildcard recomp/*.py)

all: $(PROGS:%=$(B)/%) $(PROGS:%=$(B)/%_ref) $(PROGS_ESPERADO:%=$(B)/%) $(B)/romfs_prueba.img $(LIBNX_PROGS:%=$(B)/libnx_%) $(LIBNX_PANTALLA:%=$(B)/libnx_%) $(LIBNX_GPU:%=$(B)/libnx_%) $(B)/astc_test $(if $(LIBNX_GPU),$(B)/libnx_deko_astc) $(if $(DEKO_EJEMPLOS),$(B)/libnx_deko_examples) $(if $(SHADERS),$(B)/shader_test $(SHADERS:%=$(B)/shaders/%.dksh) $(SHADERS_COMP:%=$(B)/shaders/%.dksh)) $(LIBNX_MANDOS:%=$(B)/libnx_%) $(LIBNX_AUDIO:%=$(B)/libnx_%) $(LIBNX_AUDREN:%=$(B)/libnx_%)

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
	$(CLANG) $(HOST_CFLAGS) -I$(B)/$*_rec $(B)/$*_rec/*.c $(RUNTIME) -lm -lpthread $(SDL_LIBS) -lEGL -lOpenGL -o $@

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

test: all
	@ok=1; for p in $(PROGS) $(PROGS_ESPERADO); do \
		NX_ROMFS=$(B)/romfs_prueba.img $(B)/$$p > $(B)/$$p.salida.txt; \
		if [ -f guest/esperado/$$p.txt ]; then cp guest/esperado/$$p.txt $(B)/$$p.referencia.txt; \
		else $(B)/$${p}_ref > $(B)/$$p.referencia.txt; fi; \
		if diff -u $(B)/$$p.referencia.txt $(B)/$$p.salida.txt; then echo "OK: $$p es idéntico a la referencia"; \
		else ok=0; fi; \
	done; \
	for p in $(LIBNX_PROGS); do \
		mkdir -p $(B)/sdmc; NX_SDMC=$(B)/sdmc $(B)/libnx_$$p > $(B)/$$p.salida.txt; \
		if diff -u guest/esperado/$$p.txt $(B)/$$p.salida.txt; then echo "OK: $$p (libnx) es idéntico a lo esperado"; \
		else ok=0; fi; \
	done; \
	for p in $(LIBNX_PANTALLA); do \
		NX_SIN_VENTANA=1 NX_CAPTURA=$(B)/$$p.ppm NX_CAPTURA_CUADRO=10 timeout 60 $(B)/libnx_$$p >/dev/null; \
		if [ "$$(sha256sum < $(B)/$$p.ppm | cut -d' ' -f1)" = "$$(cat guest/esperado/$$p.sha256)" ]; then \
			echo "OK: $$p (libnx, pantalla) dibuja el cuadro esperado"; else echo "MAL: $$p dibujó otro cuadro"; ok=0; fi; \
	done; \
	$(B)/astc_test 20000 >/dev/null 2>$(B)/astc.err; r=$$?; \
	if [ $$r = 0 ]; then echo "OK: el decodificador ASTC da lo mismo que Mesa en 560 mil bloques (14 tamaños, lineal y sRGB)"; \
	elif [ $$r = 77 ]; then echo "(sin el llvmpipe de Mesa no se prueba ASTC)"; else echo "MAL: ASTC distinto de Mesa"; ok=0; fi; \
	if [ -n "$(LIBNX_GPU)" ]; then \
		NX_SIN_VENTANA=1 NX_CAPTURA=$(B)/deko_astc.ppm NX_CAPTURA_CUADRO=3 timeout 60 $(B)/libnx_deko_astc >/dev/null 2>&1; \
		$(B)/astc_test --captura guest/libnx/deko_astc/romfs $(B)/deko_astc.ppm 2>>$(B)/astc.err; r=$$?; \
		if [ $$r = 0 ]; then echo "OK: deko_astc dibuja sus texturas ASTC como las decodifica Mesa"; \
		elif [ $$r != 77 ]; then echo "MAL: deko_astc dibujó otras texturas"; ok=0; fi; \
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
	done; test $$ok = 1

$(B):
	mkdir -p $@

clean:
	rm -rf $(B)

.PRECIOUS: $(M)/%.o $(M)/%.so $(B)/%.o $(B)/%.elf $(B)/%.nro $(B)/%_rec/programa.c
.PHONY: all test clean

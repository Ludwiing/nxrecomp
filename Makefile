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
RUNTIME := runtime/nxrt.c runtime/nxmain.c runtime/nxkern.c runtime/nxipc.c runtime/nxservices.c runtime/nxsvc_am.c runtime/nxsvc_hid.c runtime/nxsvc_time.c runtime/nxsvc_fs.c runtime/nxsvc_vi.c runtime/nxdisplay.c
RUNTIME_H := $(wildcard runtime/*.h)
RECOMP := $(wildcard recomp/*.py)

all: $(PROGS:%=$(B)/%) $(PROGS:%=$(B)/%_ref) $(PROGS_ESPERADO:%=$(B)/%) $(B)/romfs_prueba.img $(LIBNX_PROGS:%=$(B)/libnx_%) $(LIBNX_PANTALLA:%=$(B)/libnx_%)

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
	$(CLANG) $(HOST_CFLAGS) -I$(B)/$*_rec $(B)/$*_rec/*.c $(RUNTIME) -lm -lpthread $(SDL_LIBS) -o $@

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
	done; test $$ok = 1

$(B):
	mkdir -p $@

clean:
	rm -rf $(B)

.PRECIOUS: $(M)/%.o $(M)/%.so $(B)/%.o $(B)/%.elf $(B)/%.nro $(B)/%_rec/programa.c
.PHONY: all test clean

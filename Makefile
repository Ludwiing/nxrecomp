# guest (ARM64, formato Switch) -> NRO -> C -> ejecutable nativo de Linux x86_64
CLANG ?= clang
B := build

OPT ?= -O2
GUEST_CFLAGS := --target=aarch64-none-elf $(OPT) -ffreestanding -fno-builtin -nostdlib \
	-mgeneral-regs-only -fno-jump-tables -fno-pic -fno-stack-protector \
	-fno-asynchronous-unwind-tables -fno-unwind-tables -Wall
HOST_CFLAGS := -O2 -Iruntime -Wall -Wno-unused-label -Wno-unused-function -Wno-unused-value -Wno-parentheses-equality

all: $(B)/hola $(B)/hola_ref

$(B)/hola.elf: guest/start.S guest/hola.c guest/link.ld | $(B)
	$(CLANG) $(GUEST_CFLAGS) -c guest/start.S -o $(B)/start.o
	$(CLANG) $(GUEST_CFLAGS) -c guest/hola.c -o $(B)/hola.o
	ld.lld -T guest/link.ld -static -nostdlib $(B)/start.o $(B)/hola.o -o $@

$(B)/hola.nro: $(B)/hola.elf tools/mknro.py
	python3 tools/mknro.py $< $@

$(B)/hola_rec.c: $(B)/hola.nro recomp/nxrecomp.py recomp/a64.py
	python3 recomp/nxrecomp.py $< -o $@

$(B)/hola: $(B)/hola_rec.c runtime/nxrt.c runtime/nxrt.h
	$(CLANG) $(HOST_CFLAGS) $(B)/hola_rec.c runtime/nxrt.c -o $@

# El mismo programa compilado directo para x86_64: su salida es la referencia.
$(B)/hola_ref: guest/hola.c | $(B)
	$(CLANG) -O2 -DNATIVE_REF guest/hola.c -o $@

test: all
	@$(B)/hola > $(B)/salida_recompilada.txt
	@$(B)/hola_ref > $(B)/salida_referencia.txt
	@diff -u $(B)/salida_referencia.txt $(B)/salida_recompilada.txt && echo "OK: la salida recompilada es idéntica a la referencia"

$(B):
	mkdir -p $@

clean:
	rm -rf $(B)

.PHONY: all test clean

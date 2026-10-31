# guest (ARM64, formato Switch) -> NRO -> C -> ejecutable nativo de Linux x86_64
CLANG ?= clang
B := build
PROGS := hola neon vm

OPT ?= -O2
# -ffp-contract=off en las dos versiones: ningún compilador fusiona a*b+c por su cuenta.
GUEST_CFLAGS := --target=aarch64-none-elf $(OPT) -ffreestanding -fno-builtin -nostdlib \
	-fno-pic -fno-stack-protector -ffp-contract=off -fno-math-errno \
	-fno-asynchronous-unwind-tables -fno-unwind-tables -Wall
CFLAGS_hola := -mgeneral-regs-only
CFLAGS_neon := -O3
HOST_CFLAGS := -O2 -march=x86-64-v3 -Iruntime -Wall -Wno-unused-label -Wno-unused-function \
	-Wno-unused-value -Wno-unused-variable -Wno-parentheses-equality
RUNTIME := runtime/nxrt.c runtime/nxmain.c
RUNTIME_H := $(wildcard runtime/*.h)
RECOMP := $(wildcard recomp/*.py)

all: $(PROGS:%=$(B)/%) $(PROGS:%=$(B)/%_ref)

$(B)/start.o: guest/start.S | $(B)
	$(CLANG) $(GUEST_CFLAGS) -c $< -o $@

$(B)/%.o: guest/%.c guest/mini.h | $(B)
	$(CLANG) $(GUEST_CFLAGS) $(CFLAGS_$*) -c $< -o $@

$(B)/%.elf: $(B)/start.o $(B)/%.o guest/link.ld
	ld.lld -T guest/link.ld -static -nostdlib $(B)/start.o $(B)/$*.o -o $@

$(B)/%.nro: $(B)/%.elf tools/mknro.py
	python3 tools/mknro.py $< $@

$(B)/%_rec.c: $(B)/%.nro $(RECOMP)
	python3 recomp/nxrecomp.py $< -o $@

$(B)/%: $(B)/%_rec.c $(RUNTIME) $(RUNTIME_H)
	$(CLANG) $(HOST_CFLAGS) $< $(RUNTIME) -lm -o $@

# El mismo programa compilado directo para x86_64: su salida es la referencia.
$(B)/%_ref: guest/%.c guest/mini.h | $(B)
	$(CLANG) -O2 -ffp-contract=off -DNATIVE_REF $< -o $@

test: all
	@ok=1; for p in $(PROGS); do \
		$(B)/$$p > $(B)/$$p.salida.txt; $(B)/$${p}_ref > $(B)/$$p.referencia.txt; \
		if diff -u $(B)/$$p.referencia.txt $(B)/$$p.salida.txt; then echo "OK: $$p es idéntico a la referencia"; \
		else ok=0; fi; \
	done; test $$ok = 1

$(B):
	mkdir -p $@

clean:
	rm -rf $(B)

.PRECIOUS: $(B)/%.o $(B)/%.elf $(B)/%.nro $(B)/%_rec.c
.PHONY: all test clean

// Programa de prueba de tablas de salto: una máquina virtual de bytecode (el switch
// dentro de un bucle de cualquier intérprete), un analizador léxico y switches
// densos y dispersos. Sin -fno-jump-tables el compilador los convierte en tablas.
#include "mini.h"

enum {
    OP_HALT, OP_PUSH, OP_POP, OP_DUP, OP_SWAP, OP_OVER, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_NEG, OP_AND, OP_OR, OP_XOR, OP_SHL, OP_SHR, OP_EQ, OP_LT, OP_GT, OP_NOT, OP_JMP, OP_JZ,
    OP_JNZ, OP_LOAD, OP_STORE, OP_PRINT, OP_INC, OP_DEC, OP_CALL, OP_RET, OP_ROT,
};

static s64 mem[16];
static s64 printed_sum;
static u32 printed_count;

__attribute__((noinline)) static s64 run(const s32 *code, u32 *steps) {
    s64 st[64];
    int sp = 0, pc = 0, rsp = 0;
    int rstack[16];
    for (;;) {
        (*steps)++;
        s32 op = code[pc++];
        switch (op) {
        case OP_HALT: return sp ? st[sp - 1] : 0;
        case OP_PUSH: st[sp++] = code[pc++]; break;
        case OP_POP: sp--; break;
        case OP_DUP: st[sp] = st[sp - 1]; sp++; break;
        case OP_SWAP: { s64 t = st[sp - 1]; st[sp - 1] = st[sp - 2]; st[sp - 2] = t; } break;
        case OP_OVER: st[sp] = st[sp - 2]; sp++; break;
        case OP_ADD: sp--; st[sp - 1] += st[sp]; break;
        case OP_SUB: sp--; st[sp - 1] -= st[sp]; break;
        case OP_MUL: sp--; st[sp - 1] *= st[sp]; break;
        case OP_DIV: sp--; st[sp - 1] = st[sp] ? st[sp - 1] / st[sp] : 0; break;
        case OP_MOD: sp--; st[sp - 1] = st[sp] ? st[sp - 1] % st[sp] : 0; break;
        case OP_NEG: st[sp - 1] = -st[sp - 1]; break;
        case OP_AND: sp--; st[sp - 1] &= st[sp]; break;
        case OP_OR: sp--; st[sp - 1] |= st[sp]; break;
        case OP_XOR: sp--; st[sp - 1] ^= st[sp]; break;
        case OP_SHL: sp--; st[sp - 1] <<= (st[sp] & 63); break;
        case OP_SHR: sp--; st[sp - 1] >>= (st[sp] & 63); break;
        case OP_EQ: sp--; st[sp - 1] = st[sp - 1] == st[sp]; break;
        case OP_LT: sp--; st[sp - 1] = st[sp - 1] < st[sp]; break;
        case OP_GT: sp--; st[sp - 1] = st[sp - 1] > st[sp]; break;
        case OP_NOT: st[sp - 1] = !st[sp - 1]; break;
        case OP_JMP: pc = code[pc]; break;
        case OP_JZ: pc = st[--sp] ? pc + 1 : code[pc]; break;
        case OP_JNZ: pc = st[--sp] ? code[pc] : pc + 1; break;
        case OP_LOAD: st[sp++] = mem[code[pc++] & 15]; break;
        case OP_STORE: mem[code[pc++] & 15] = st[--sp]; break;
        case OP_PRINT: printed_sum += st[--sp]; printed_count++; break;
        case OP_INC: st[sp - 1]++; break;
        case OP_DEC: st[sp - 1]--; break;
        case OP_CALL: rstack[rsp++] = pc + 1; pc = code[pc]; break;
        case OP_RET: pc = rstack[--rsp]; break;
        case OP_ROT: { s64 t = st[sp - 3]; st[sp - 3] = st[sp - 2]; st[sp - 2] = st[sp - 1]; st[sp - 1] = t; } break;
        default: return -999;
        }
    }
}

// Programa: suma los primos menores que 2000 (división de prueba) y la secuencia de Collatz de 27.
static const s32 primes_prog[] = {
    OP_PUSH, 2, OP_STORE, 0,             // 0: n = 2
    OP_PUSH, 0, OP_STORE, 1,             // 4: suma = 0
    /* 8 */ OP_LOAD, 0, OP_PUSH, 2000, OP_LT, OP_JZ, 58,
    /* 15 */ OP_PUSH, 2, OP_STORE, 2,    // d = 2
    /* 19 */ OP_LOAD, 2, OP_DUP, OP_MUL, OP_LOAD, 0, OP_GT, OP_JNZ, 44, // d*d > n → primo
    /* 28 */ OP_LOAD, 0, OP_LOAD, 2, OP_MOD, OP_JZ, 50, // n % d == 0 → no primo
    /* 35 */ OP_LOAD, 2, OP_INC, OP_STORE, 2, OP_JMP, 19, 0, 0,
    /* 44 */ OP_LOAD, 1, OP_LOAD, 0, OP_ADD, OP_STORE, 1, // suma += n (ocupa 44..50)
    /* 50... reubicado abajo */
};

static s32 prog[256];

static int build_program(void) {
    int n = 0;
    // Se arma a mano con etiquetas para no contar posiciones.
#define E(x) prog[n++] = (x)
    E(OP_PUSH); E(2); E(OP_STORE); E(0);
    E(OP_PUSH); E(0); E(OP_STORE); E(1);
    int loop = n;
    E(OP_LOAD); E(0); E(OP_PUSH); E(2000); E(OP_LT); E(OP_JZ); int jz_end = n; E(0);
    E(OP_PUSH); E(2); E(OP_STORE); E(2);
    int inner = n;
    E(OP_LOAD); E(2); E(OP_DUP); E(OP_MUL); E(OP_LOAD); E(0); E(OP_GT); E(OP_JNZ); int jprime = n; E(0);
    E(OP_LOAD); E(0); E(OP_LOAD); E(2); E(OP_MOD); E(OP_JZ); int jnot = n; E(0);
    E(OP_LOAD); E(2); E(OP_INC); E(OP_STORE); E(2); E(OP_JMP); E(inner);
    prog[jprime] = n;
    E(OP_LOAD); E(1); E(OP_LOAD); E(0); E(OP_ADD); E(OP_STORE); E(1);
    E(OP_LOAD); E(0); E(OP_PRINT);
    prog[jnot] = n;
    E(OP_LOAD); E(0); E(OP_INC); E(OP_STORE); E(0); E(OP_JMP); E(loop);
    prog[jz_end] = n;
    // Collatz de 27 con una subrutina que cuenta pasos
    E(OP_PUSH); E(27); E(OP_STORE); E(3); E(OP_PUSH); E(0); E(OP_STORE); E(4);
    int col = n;
    E(OP_LOAD); E(3); E(OP_PUSH); E(1); E(OP_EQ); E(OP_JNZ); int jdone = n; E(0);
    E(OP_CALL); int jcall = n; E(0);
    E(OP_LOAD); E(3); E(OP_PUSH); E(1); E(OP_AND); E(OP_JZ); int jeven = n; E(0);
    E(OP_LOAD); E(3); E(OP_PUSH); E(3); E(OP_MUL); E(OP_INC); E(OP_STORE); E(3); E(OP_JMP); E(col);
    prog[jeven] = n;
    E(OP_LOAD); E(3); E(OP_PUSH); E(1); E(OP_SHR); E(OP_STORE); E(3); E(OP_JMP); E(col);
    prog[jcall] = n;
    E(OP_LOAD); E(4); E(OP_INC); E(OP_STORE); E(4); E(OP_RET);
    prog[jdone] = n;
    E(OP_LOAD); E(1); E(OP_LOAD); E(4); E(OP_PUSH); E(7); E(OP_ROT); E(OP_XOR); E(OP_NEG); E(OP_SWAP); E(OP_OVER);
    E(OP_SUB); E(OP_PUSH); E(5); E(OP_DIV); E(OP_ADD); E(OP_HALT);
#undef E
    return n;
}

// Analizador léxico: clasifica cada carácter con un switch denso.
enum { T_LETRA, T_DIGITO, T_ESPACIO, T_OPERADOR, T_PUNTUACION, T_OTRO };

__attribute__((noinline)) static int clase(char ch) {
    switch (ch) {
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i':
    case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
    case 's': case 't': case 'u': case 'v': case 'w': case 'x': case 'y': case 'z': case '_':
        return T_LETRA;
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
        return T_DIGITO;
    case ' ': case '\t': case '\n': return T_ESPACIO;
    case '+': case '-': case '*': case '/': case '=': case '<': case '>': case '!': return T_OPERADOR;
    case '(': case ')': case '{': case '}': case ';': case ',': return T_PUNTUACION;
    default: return T_OTRO;
    }
}

// Un switch con casos que hacen cosas distintas (no se puede convertir en una tabla de valores).
__attribute__((noinline)) static u64 accion(int k, u64 x) {
    switch (k) {
    case 0: return x + 1;
    case 1: return x * 3;
    case 2: return x ^ 0x5555;
    case 3: return x >> 2;
    case 4: return x << 3;
    case 5: return ~x;
    case 6: return x * x;
    case 7: return x / 7;
    case 8: return x % 1000;
    case 9: return x - 12345;
    case 10: return (x << 7) | (x >> 57);
    case 11: return x & 0xFFFF;
    default: return 0;
    }
}

// Switch disperso: el compilador usa comparaciones, no tabla.
__attribute__((noinline)) static int disperso(int k) {
    switch (k) {
    case 3: return 30;
    case 100: return 1000;
    case 2026: return 31;
    case 99999: return 7;
    case -5: return -50;
    default: return 0;
    }
}

int nx_main(void) {
    pr("Máquina virtual de bytecode en la Switch, recompilada a x86_64.\n");
    int len = build_program();
    u32 steps = 0;
    s64 r = run(prog, &steps);
    pr("programa de %d palabras: resultado %D en %u pasos\n", len, r, steps);
    pr("primos impresos: %u, suma %D, suma guardada %D, pasos de Collatz(27): %D\n", printed_count, printed_sum,
       mem[1], mem[4]);

    const char *src = "int main() { return fib(27) + x_2 * (y - 3); }\n";
    int counts[6] = {0};
    for (const char *p = src; *p; p++) counts[clase(*p)]++;
    pr("léxico: %d letras, %d dígitos, %d espacios, %d operadores, %d puntuación, %d otros\n", counts[0], counts[1],
       counts[2], counts[3], counts[4], counts[5]);

    u64 x = 0x1234567;
    for (int i = 0; i < 1000; i++) x = accion((int)(x % 13), x) + (u64)i;
    pr("1000 acciones: 0x%X\n", x);

    int ks[] = {3, 100, 2026, 99999, -5, 42};
    s32 s = 0;
    for (int i = 0; i < 6; i++) s = s * 10 + disperso(ks[i]);
    pr("disperso: %d\n", s);

    // y la máquina virtual con un opcode inválido
    s32 bad[] = {OP_PUSH, 1, 77};
    steps = 0;
    pr("opcode inválido: %D\n", run(bad, &steps));
    pr("Fin.\n");
    flush();
    (void)primes_prog;
    return 0;
}

#ifdef NATIVE_REF
int main(void) { return nx_main(); }
#endif

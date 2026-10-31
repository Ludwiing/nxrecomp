// Referencia nativa del par main + sdk: los dos módulos en un solo programa de x86_64.
// Los constructores los corre la libc antes de main(), igual que rtld antes de nnMain.
#include <stdio.h>
void funcion_que_no_existe(void) {}
int nnMain(void);
int main(void) { return nnMain(); }

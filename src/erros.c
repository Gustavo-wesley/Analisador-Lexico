#include <string.h>
#include "erros.h"

Erro erros[MAX_ERROS];
int qtdErros = 0;

void limparErros(void) {
    qtdErros = 0;
}

void adicionarErro(int linha, int coluna, int tamanho, const char *mensagem) {
    Erro *e;

    if (qtdErros >= MAX_ERROS) return;

    e = &erros[qtdErros++];
    e->linha = linha;
    e->coluna = coluna;
    e->tamanho = tamanho < 1 ? 1 : tamanho;
    strncpy(e->mensagem, mensagem, sizeof(e->mensagem) - 1);
    e->mensagem[sizeof(e->mensagem) - 1] = '\0';
}

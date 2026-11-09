#include <string.h>
#include "erros.h"

Erro erros[MAX_ERROS];
int qtdErros = 0;

void limparErros(void) {
    qtdErros = 0;
}

void adicionarErroFase(Fase fase, int linha, int coluna, int tamanho, const char *mensagem) {
    Erro *e;

    if (qtdErros >= MAX_ERROS) return;

    e = &erros[qtdErros++];
    e->linha = linha;
    e->coluna = coluna;
    e->tamanho = tamanho < 1 ? 1 : tamanho;
    e->fase = fase;
    strncpy(e->mensagem, mensagem, sizeof(e->mensagem) - 1);
    e->mensagem[sizeof(e->mensagem) - 1] = '\0';
}

void adicionarErro(int linha, int coluna, int tamanho, const char *mensagem) {
    adicionarErroFase(FASE_LEXICA, linha, coluna, tamanho, mensagem);
}

int contarErros(Fase fase) {
    int i, n = 0;
    for (i = 0; i < qtdErros; i++) {
        if (erros[i].fase == fase) n++;
    }
    return n;
}

const char *nomeFase(Fase fase) {
    switch (fase) {
        case FASE_LEXICA:    return "lexico";
        case FASE_SINTATICA: return "sintatico";
        case FASE_SEMANTICA: return "semantico";
    }
    return "?";
}

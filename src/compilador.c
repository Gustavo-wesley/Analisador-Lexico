#include <stddef.h>
#include "compilador.h"
#include "lexer.h"
#include "sintatico.h"
#include "semantico.h"
#include "arvore.h"
#include "erros.h"

int semanticaExecutada = 0;
Fase faseFinal = FASE_SEMANTICA;

int faseLigada(Fase fase) {
    return fase <= faseFinal;
}

/* ordena os erros por posicao (insercao: estavel e os vetores sao pequenos) */
static void ordenarErros(void) {
    int i, j;
    for (i = 1; i < qtdErros; i++) {
        Erro e = erros[i];
        j = i - 1;
        while (j >= 0 && (erros[j].linha > e.linha ||
                          (erros[j].linha == e.linha && erros[j].coluna > e.coluna))) {
            erros[j + 1] = erros[j];
            j--;
        }
        erros[j + 1] = e;
    }
}

void compilarFonte(const char *texto) {
    analisarFonte(texto);      /* tambem limpa os erros e a tabela de simbolos */

    if (faseLigada(FASE_SINTATICA)) analisarSintaxe();
    else limparArvore();

    semanticaExecutada = 0;
    if (faseLigada(FASE_SEMANTICA) && contarErros(FASE_SINTATICA) == 0 && raiz != NULL) {
        analisarSemantica();
        semanticaExecutada = 1;
    } else {
        limparSemantica();
    }

    gerarLinhasArvore();
    ordenarErros();
}

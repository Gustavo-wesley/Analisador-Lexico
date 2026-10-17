/* Versao de console do analisador lexico do Pasquick.
   Uso: pasquick_lex arquivo.pqk
   Serve para testar o lexer antes da IDE Chronus ficar pronta. */

#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "simbolos.h"
#include "erros.h"

static char *lerArquivo(const char *caminho) {
    FILE *f = fopen(caminho, "rb");
    long tam;
    char *buffer;

    if (f == NULL) return NULL;

    fseek(f, 0, SEEK_END);
    tam = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = (char *)malloc(tam + 1);
    if (buffer == NULL) {
        fclose(f);
        return NULL;
    }
    fread(buffer, 1, tam, f);
    buffer[tam] = '\0';
    fclose(f);
    return buffer;
}

static void imprimirTokens(void) {
    int i;

    printf("\n=== TOKENS (%d) ===\n", qtdTokens);
    printf("%-9s %-15s %-20s %s\n", "LIN:COL", "TOKEN", "LEXEMA", "ATRIBUTO");
    for (i = 0; i < qtdTokens; i++) {
        Token *t = &tokens[i];
        char posicao[16];

        sprintf(posicao, "%d:%d", t->linha, t->coluna);
        printf("%-9s %-15s %-20s ", posicao, nomeToken(t->tipo), t->lexema);

        if (t->tipo == TK_NUM)
            printf("%s, valor = %g\n", t->ehReal ? "real" : "inteiro", t->valor);
        else if (t->indiceTS >= 0)
            printf("TS[%d]\n", t->indiceTS);
        else
            printf("-\n");
    }
}

static void imprimirTabelaSimbolos(void) {
    int i;

    printf("\n=== TABELA DE SIMBOLOS (%d) ===\n", qtdSimbolos);
    printf("%-5s %-16s %-15s %-18s %s\n", "IND", "LEXEMA", "TOKEN", "CATEGORIA", "LINHA");
    for (i = 0; i < qtdSimbolos; i++) {
        Simbolo *s = &tabelaSimbolos[i];
        printf("%-5d %-16s %-15s %-18s ", i, s->lexema, nomeToken(s->tipo),
               nomeCategoria(s->categoria));
        if (s->linha > 0) printf("%d\n", s->linha);
        else printf("-\n");
    }
}

static void imprimirErros(void) {
    int i;

    printf("\n=== ERROS LEXICOS (%d) ===\n", qtdErros);
    for (i = 0; i < qtdErros; i++) {
        printf("Linha %d, coluna %d: %s\n", erros[i].linha, erros[i].coluna, erros[i].mensagem);
    }
}

int main(int argc, char *argv[]) {
    char *texto;

    if (argc < 2) {
        printf("Uso: %s arquivo.pqk\n", argv[0]);
        return 1;
    }

    texto = lerArquivo(argv[1]);
    if (texto == NULL) {
        printf("Nao foi possivel abrir '%s'\n", argv[1]);
        return 1;
    }

    analisarFonte(texto);

    imprimirTokens();
    imprimirTabelaSimbolos();
    imprimirErros();

    free(texto);
    return qtdErros > 0 ? 2 : 0;
}

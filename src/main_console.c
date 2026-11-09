/* Versao de console do compilador Pasquick (lexico + sintatico + semantico).

   Uso: pasquick arquivo.pqk [-f fase] [-t] [-s] [-a] [-d]
     -f  ate qual fase compilar: lexico, sintatico ou semantico (padrao)
     -t  lista de tokens
     -s  tabela de simbolos (lexer)
     -a  arvore sintatica (com -f semantico, mostra tambem os tipos)
     -d  tabela de declaracoes (escopos e tipos)
   Sem -t/-s/-a/-d mostra tudo; com -f, mostra o resultado daquela fase.
   Os erros sao sempre mostrados.
   Retorna 0 se o programa esta correto e 2 se tem erro. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "simbolos.h"
#include "erros.h"
#include "arvore.h"
#include "semantico.h"
#include "compilador.h"

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
    tam = (long)fread(buffer, 1, tam, f);
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

static void imprimirArvore(void) {
    int i;

    printf("\n=== ARVORE SINTATICA (%d linhas) ===\n", qtdLinhasArvore);
    if (!faseLigada(FASE_SINTATICA)) {
        printf("(analise sintatica desligada: use -f sintatico ou -f semantico)\n");
        return;
    }
    for (i = 0; i < qtdLinhasArvore; i++) printf("%s\n", linhasArvore[i].texto);
}

static void imprimirDeclaracoes(void) {
    int i;

    printf("\n=== TABELA DE DECLARACOES (%d) ===\n", qtdDeclaracoes);
    if (!faseLigada(FASE_SEMANTICA)) {
        printf("(analise semantica desligada: use -f semantico)\n");
        return;
    }
    if (!semanticaExecutada) {
        printf("(analise semantica nao executada: corrija primeiro os erros sintaticos)\n");
        return;
    }
    printf("%-4s %-16s %-13s %-12s %-16s %-6s %-6s %s\n",
           "IND", "NOME", "CATEGORIA", "TIPO", "ESCOPO", "NIVEL", "LINHA", "USADA");
    for (i = 0; i < qtdDeclaracoes; i++) {
        Declaracao *d = &declaracoes[i];
        char tipo[48];

        if (d->categoria == DEC_PROCEDIMENTO || d->categoria == DEC_PROGRAMA) strcpy(tipo, "-");
        else strcpy(tipo, nomeTipo(d->tipo));

        printf("%-4d %-16s %-13s %-12s %-16s %-6d ", i, d->nome, nomeCategoriaDec(d->categoria),
               tipo, d->escopo, d->nivel);
        if (d->predefinida) printf("%-6s %s\n", "-", "pre-definido");
        else printf("%-6d %s\n", d->linha, d->usada ? "sim" : "nao");
    }
}

static void imprimirErros(void) {
    int i;

    printf("\n=== ERROS ate a fase %s (%d:", nomeFase(faseFinal), qtdErros);
    printf(" %d lexico(s)", contarErros(FASE_LEXICA));
    if (faseLigada(FASE_SINTATICA)) printf(", %d sintatico(s)", contarErros(FASE_SINTATICA));
    if (faseLigada(FASE_SEMANTICA)) printf(", %d semantico(s)", contarErros(FASE_SEMANTICA));
    printf(") ===\n");
    for (i = 0; i < qtdErros; i++) {
        printf("Linha %d, coluna %d [%s]: %s\n", erros[i].linha, erros[i].coluna,
               nomeFase(erros[i].fase), erros[i].mensagem);
    }
    if (qtdErros == 0) {
        if (faseFinal == FASE_LEXICA) printf("Nenhum erro lexico.\n");
        else if (faseFinal == FASE_SINTATICA) printf("Programa sintaticamente correto.\n");
        else printf("Programa correto.\n");
    }
    else if (faseLigada(FASE_SEMANTICA) && !semanticaExecutada) printf("(analise semantica nao executada por causa dos erros sintaticos)\n");
}

int main(int argc, char *argv[]) {
    char *texto;
    const char *caminho = NULL;
    int verTokens = 0, verSimbolos = 0, verArvore = 0, verDeclaracoes = 0;
    int escolheuFase = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0) verTokens = 1;
        else if (strcmp(argv[i], "-s") == 0) verSimbolos = 1;
        else if (strcmp(argv[i], "-a") == 0) verArvore = 1;
        else if (strcmp(argv[i], "-d") == 0) verDeclaracoes = 1;
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            const char *f = argv[++i];
            escolheuFase = 1;
            if (strcmp(f, "lexico") == 0) faseFinal = FASE_LEXICA;
            else if (strcmp(f, "sintatico") == 0) faseFinal = FASE_SINTATICA;
            else if (strcmp(f, "semantico") == 0) faseFinal = FASE_SEMANTICA;
            else {
                printf("Fase desconhecida '%s' (use lexico, sintatico ou semantico)\n", f);
                return 1;
            }
        }
        else caminho = argv[i];
    }

    if (caminho == NULL) {
        printf("Uso: %s arquivo.pqk [-f fase] [-t] [-s] [-a] [-d]\n", argv[0]);
        printf("  -f lexico | sintatico | semantico   ate qual fase compilar (padrao: semantico)\n");
        printf("  -t tokens   -s tabela de simbolos   -a arvore   -d declaracoes\n");
        printf("  sem -t/-s/-a/-d mostra tudo (ou so o da fase escolhida com -f)\n");
        return 1;
    }
    if (!verTokens && !verSimbolos && !verArvore && !verDeclaracoes) {
        if (!escolheuFase) {
            verTokens = verSimbolos = verArvore = verDeclaracoes = 1;
        } else if (faseFinal == FASE_LEXICA) {
            verTokens = verSimbolos = 1;       /* resultado do lexico */
        } else if (faseFinal == FASE_SINTATICA) {
            verArvore = 1;                     /* resultado do sintatico */
        } else {
            verArvore = verDeclaracoes = 1;    /* arvore com tipos + escopos */
        }
    }

    texto = lerArquivo(caminho);
    if (texto == NULL) {
        printf("Nao foi possivel abrir '%s'\n", caminho);
        return 1;
    }

    compilarFonte(texto);

    if (verTokens) imprimirTokens();
    if (verSimbolos) imprimirTabelaSimbolos();
    if (verArvore) imprimirArvore();
    if (verDeclaracoes) imprimirDeclaracoes();
    imprimirErros();

    free(texto);
    return qtdErros > 0 ? 2 : 0;
}

#ifndef SEMANTICO_H
#define SEMANTICO_H

#include "arvore.h"

/* Categoria de um nome declarado no programa */
typedef enum {
    DEC_PROGRAMA,
    DEC_VARIAVEL,
    DEC_CONSTANTE,
    DEC_PARAMETRO,
    DEC_PROCEDIMENTO,
    DEC_FUNCAO
} CategoriaDec;

/* Tabela de declaracoes do analisador semantico.
   Diferente da tabela de simbolos do lexer (um registro por lexema), aqui
   cada declaracao tem o seu escopo: o mesmo nome pode aparecer como variavel
   global e como parametro de uma funcao. */
typedef struct {
    char nome[32];
    CategoriaDec categoria;
    TipoDado tipo;         /* tipo da variavel/constante/parametro ou retorno da funcao */
    char escopo[32];       /* "global" ou o nome da sub-rotina */
    int nivel;             /* 0 = global, 1 = dentro de uma sub-rotina, ... */
    int linha, coluna;     /* 0 para os pre-definidos */
    int primeiroParam;     /* sub-rotinas: indice do primeiro parametro em declaracoes[] */
    int qtdParams;         /* -1 = quantidade variavel (write, read...) */
    int predefinida;       /* 1 para write, writeln, read, readln */
    int usada;             /* foi usada em algum lugar */
} Declaracao;

#define MAX_DECLARACOES 2000

extern Declaracao declaracoes[MAX_DECLARACOES];
extern int qtdDeclaracoes;

/* Percorre a arvore em `raiz`: monta a tabela de declaracoes, anota o tipo
   de cada expressao e registra os erros semanticos (FASE_SEMANTICA). */
void analisarSemantica(void);

/* So limpa a tabela (usado quando a analise semantica nao roda) */
void limparSemantica(void);

const char *nomeCategoriaDec(CategoriaDec c);

#endif

#ifndef ARVORE_H
#define ARVORE_H

#include "tokens.h"

/* ---------- tipos de dados da linguagem ---------- */

typedef enum {
    T_NENHUM,    /* ainda sem tipo (ou comando, que nao tem tipo) */
    T_ERRO,      /* expressao com erro: nao gera mais mensagens em cascata */
    T_INTEGER,
    T_REAL,
    T_CHAR,
    T_STRING,
    T_LOGICO     /* resultado de comparacoes, E e OU (nao existe variavel logica) */
} TipoBase;

typedef struct {
    TipoBase base;
    int vetor;       /* 1 se for vetor: integer[10] */
    int tamanho;     /* quantidade de elementos (-1 se o tamanho escrito for invalido) */
} TipoDado;

/* ---------- arvore sintatica abstrata (AST) ---------- */

typedef enum {
    NO_PROGRAMA,      /* tk = nome;    filho[0] = declaracoes, filho[1] = bloco principal */
    NO_VAR,           /* tk = nome;    tipoDeclarado */
    NO_CONST,         /* tk = nome;    filho[0] = valor, tipoDeclarado (T_NENHUM se omitido) */
    NO_PROCEDIMENTO,  /* tk = nome;    filho[0] = parametros, filho[1] = declaracoes, filho[2] = bloco */
    NO_FUNCAO,        /* igual ao procedimento + tipoDeclarado = tipo de retorno */
    NO_PARAMETRO,     /* tk = nome;    tipoDeclarado */

    NO_BLOCO,         /* begin ... end: filho[0] = lista de comandos */
    NO_ATRIB,         /* tk = ':=';    filho[0] = destino (NO_ID ou NO_INDICE), filho[1] = expressao */
    NO_CHAMADA,       /* tk = nome;    filho[0] = lista de argumentos */
    NO_IF,            /* filho[0] = condicao, filho[1] = then, filho[2] = else (ou NULL) */
    NO_WHILE,         /* filho[0] = condicao, filho[1] = corpo */
    NO_REPEAT,        /* filho[0] = lista de comandos, filho[1] = condicao */
    NO_FOR,           /* filho[0] = variavel (NO_ID), filho[1] = inicio, filho[2] = fim, filho[3] = corpo */
    NO_BREAK,
    NO_CONTINUE,

    NO_BINARIO,       /* tk = operador; op; filho[0] e filho[1] */
    NO_UNARIO,        /* tk = operador; op; filho[0] */
    NO_NUM,
    NO_LITERAL,
    NO_ID,            /* uso de um identificador */
    NO_INDICE,        /* tk = nome do vetor/string; filho[0] = indice */

    NO_QTD_TIPOS
} TipoNo;

typedef struct No {
    TipoNo tipo;
    int tk;                  /* indice em tokens[] (linha, coluna e lexema vem dele) */
    TipoToken op;            /* operador, para NO_BINARIO e NO_UNARIO */
    TipoDado tipoDeclarado;  /* tipo escrito no fonte (declaracoes) */
    TipoDado tipoDado;       /* tipo calculado pelo analisador semantico (expressoes) */
    int declaracao;          /* indice em declaracoes[] ligado pelo semantico (-1 se nenhum) */
    struct No *filho[4];
    struct No *prox;         /* proximo elemento da lista (comandos, declaracoes, argumentos) */
} No;

#define MAX_NOS 30000

extern No *raiz;

void limparArvore(void);
No  *novoNo(TipoNo tipo, int tk);

/* Nome do identificador em MAIUSCULO (a linguagem nao diferencia) */
const char *nomeId(int tk);

const char *nomeTipo(TipoDado t);   /* "integer", "real[10]", ... */
TipoDado tipoSimples(TipoBase base);

/* ---------- arvore em texto, para o console e para a aba da IDE ---------- */

#define MAX_LINHAS_ARVORE 8000

typedef struct {
    char texto[100];
    int linha;
    int coluna;
} LinhaArvore;

extern LinhaArvore linhasArvore[MAX_LINHAS_ARVORE];
extern int qtdLinhasArvore;

void gerarLinhasArvore(void);

#endif

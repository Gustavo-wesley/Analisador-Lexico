#ifndef LEXER_H
#define LEXER_H

#include "tokens.h"

#define MAX_TOKENS  10000
#define MAX_LEXEMA  128

typedef struct {
    TipoToken tipo;
    char lexema[MAX_LEXEMA];  /* texto como foi escrito no fonte */
    int linha;
    int coluna;
    int tamanho;              /* em caracteres */
    int inicio;               /* posicao (byte) no texto fonte, usada para colorir na IDE */
    int bytes;                /* quantos bytes o lexema ocupa no fonte */
    int indiceTS;             /* posicao na tabela de simbolos (-1 se nao tem) */
    double valor;             /* valor numerico (so para TK_NUM) */
    int ehReal;               /* 1 se o NUM tem parte decimal */
} Token;

extern Token tokens[MAX_TOKENS];
extern int qtdTokens;

/* Analisa o texto inteiro: preenche tokens[], a tabela de simbolos e os erros.
   Pode ser chamada varias vezes (a IDE chama a cada tecla). */
void analisarFonte(const char *texto);

#endif

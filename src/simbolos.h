#ifndef SIMBOLOS_H
#define SIMBOLOS_H

#include "tokens.h"

#define MAX_SIMBOLOS 1000
#define MAX_TAM_ID   15   /* tamanho maximo de um identificador */

typedef enum {
    CAT_RESERVADA,
    CAT_OPERADOR,
    CAT_PONTUACAO,
    CAT_IDENTIFICADOR
} Categoria;

typedef struct {
    char lexema[32];      /* sempre em MAIUSCULO (a linguagem nao diferencia) */
    TipoToken tipo;
    Categoria categoria;
    int linha;            /* linha onde o identificador apareceu primeiro (0 = pre-carregado) */
} Simbolo;

extern Simbolo tabelaSimbolos[MAX_SIMBOLOS];
extern int qtdSimbolos;

/* Limpa a tabela e carrega palavras reservadas, operadores e pontuacao */
void iniciarTabelaSimbolos(void);

/* Retorna o indice do lexema na tabela ou -1 se nao existir */
int buscarSimbolo(const char *lexema);

/* Insere e retorna o indice (ou -1 se a tabela estiver cheia) */
int inserirSimbolo(const char *lexema, TipoToken tipo, Categoria categoria, int linha);

const char *nomeCategoria(Categoria categoria);

#endif

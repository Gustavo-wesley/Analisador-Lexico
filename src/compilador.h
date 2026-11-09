#ifndef COMPILADOR_H
#define COMPILADOR_H

#include "erros.h"

/* Roda as fases do compilador em sequencia sobre o texto:
     1. lexica     (lexer.c)     -> tokens[], tabelaSimbolos[]
     2. sintatica  (sintatico.c) -> arvore em `raiz`
     3. semantica  (semantico.c) -> declaracoes[] e tipos anotados na arvore
   Todos os erros vao para erros[], ordenados por linha e coluna.
   A IDE chama esta funcao a cada tecla; o console chama uma vez. */
void compilarFonte(const char *texto);

/* 1 se a fase semantica rodou (ela so roda quando nao ha erro sintatico,
   para nao gerar avisos falsos sobre um programa que nem foi entendido) */
extern int semanticaExecutada;

/* Ate qual fase o compilador roda (padrao: FASE_SEMANTICA, o compilador todo).
   Serve para apresentar o trabalho por partes: so o lexico, depois o
   sintatico e por fim o semantico. */
extern Fase faseFinal;

/* 1 se a fase esta ligada (faseFinal chega ate ela) */
int faseLigada(Fase fase);

#endif

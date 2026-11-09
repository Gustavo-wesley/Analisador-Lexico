#ifndef SINTATICO_H
#define SINTATICO_H

#include "arvore.h"

/* Analisador sintatico descendente recursivo (um procedimento por regra da
   gramatica, ver o comentario no inicio de sintatico.c).

   Le tokens[] gerados pelo lexer, monta a arvore sintatica em `raiz` e
   registra os erros sintaticos em erros[] (fase FASE_SINTATICA).
   Usa recuperacao em modo panico, entao continua depois de cada erro. */
void analisarSintaxe(void);

#endif

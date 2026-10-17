#include "tokens.h"

/* Nome de cada token, na mesma ordem do enum TipoToken */
static const char *nomes[TK_QTD_TIPOS] = {
    "PROGRAM", "BEGIN", "END", "CONST", "VAR", "INTEGER", "REAL", "CHAR",
    "STRING", "PROCEDURE", "FUNCTION", "IF", "THEN", "ELSE", "WHILE", "DO",
    "REPEAT", "UNTIL", "BREAK", "CONTINUE", "E", "OU", "FOR", "TO",
    "ID", "NUM", "LITERAL",
    "ATRIB", "MAIS", "MENOS", "MULT", "DIV", "IGUAL", "DIFERENTE",
    "MENOR", "MENOR_IGUAL", "MAIOR", "MAIOR_IGUAL",
    "PONTO_VIRGULA", "VIRGULA", "PONTO", "DOIS_PONTOS",
    "ABRE_PAR", "FECHA_PAR", "ABRE_COL", "FECHA_COL",
    "EOF"
};

const char *nomeToken(TipoToken tipo) {
    if (tipo < 0 || tipo >= TK_QTD_TIPOS) return "?";
    return nomes[tipo];
}

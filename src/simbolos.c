#include <string.h>
#include "simbolos.h"

Simbolo tabelaSimbolos[MAX_SIMBOLOS];
int qtdSimbolos = 0;

typedef struct {
    const char *lexema;
    TipoToken tipo;
    Categoria categoria;
} SimboloInicial;

/* Tokens conhecidos antes de ler o programa */
static const SimboloInicial iniciais[] = {
    { "PROGRAM",   TK_PROGRAM,   CAT_RESERVADA },
    { "BEGIN",     TK_BEGIN,     CAT_RESERVADA },
    { "END",       TK_END,       CAT_RESERVADA },
    { "CONST",     TK_CONST,     CAT_RESERVADA },
    { "VAR",       TK_VAR,       CAT_RESERVADA },
    { "INTEGER",   TK_INTEGER,   CAT_RESERVADA },
    { "REAL",      TK_REAL,      CAT_RESERVADA },
    { "CHAR",      TK_CHAR,      CAT_RESERVADA },
    { "STRING",    TK_STRING,    CAT_RESERVADA },
    { "PROCEDURE", TK_PROCEDURE, CAT_RESERVADA },
    { "FUNCTION",  TK_FUNCTION,  CAT_RESERVADA },
    { "IF",        TK_IF,        CAT_RESERVADA },
    { "THEN",      TK_THEN,      CAT_RESERVADA },
    { "ELSE",      TK_ELSE,      CAT_RESERVADA },
    { "WHILE",     TK_WHILE,     CAT_RESERVADA },
    { "DO",        TK_DO,        CAT_RESERVADA },
    { "REPEAT",    TK_REPEAT,    CAT_RESERVADA },
    { "UNTIL",     TK_UNTIL,     CAT_RESERVADA },
    { "BREAK",     TK_BREAK,     CAT_RESERVADA },
    { "CONTINUE",  TK_CONTINUE,  CAT_RESERVADA },
    { "E",         TK_E,         CAT_RESERVADA },
    { "OU",        TK_OU,        CAT_RESERVADA },
    { "FOR",       TK_FOR,       CAT_RESERVADA },
    { "TO",        TK_TO,        CAT_RESERVADA },

    { ":=", TK_ATRIB,       CAT_OPERADOR },
    { "+",  TK_MAIS,        CAT_OPERADOR },
    { "-",  TK_MENOS,       CAT_OPERADOR },
    { "*",  TK_MULT,        CAT_OPERADOR },
    { "/",  TK_DIV,         CAT_OPERADOR },
    { "=",  TK_IGUAL,       CAT_OPERADOR },
    { "<>", TK_DIFERENTE,   CAT_OPERADOR },
    { "<",  TK_MENOR,       CAT_OPERADOR },
    { "<=", TK_MENOR_IGUAL, CAT_OPERADOR },
    { ">",  TK_MAIOR,       CAT_OPERADOR },
    { ">=", TK_MAIOR_IGUAL, CAT_OPERADOR },

    { ";", TK_PONTO_VIRGULA, CAT_PONTUACAO },
    { ",", TK_VIRGULA,       CAT_PONTUACAO },
    { ".", TK_PONTO,         CAT_PONTUACAO },
    { ":", TK_DOIS_PONTOS,   CAT_PONTUACAO },
    { "(", TK_ABRE_PAR,      CAT_PONTUACAO },
    { ")", TK_FECHA_PAR,     CAT_PONTUACAO },
    { "[", TK_ABRE_COL,      CAT_PONTUACAO },
    { "]", TK_FECHA_COL,     CAT_PONTUACAO },
};

void iniciarTabelaSimbolos(void) {
    int i;
    int qtd = sizeof(iniciais) / sizeof(iniciais[0]);

    qtdSimbolos = 0;
    for (i = 0; i < qtd; i++) {
        inserirSimbolo(iniciais[i].lexema, iniciais[i].tipo, iniciais[i].categoria, 0);
    }
}

/* Busca linear: simples e suficiente para o tamanho dos programas do trabalho */
int buscarSimbolo(const char *lexema) {
    int i;
    for (i = 0; i < qtdSimbolos; i++) {
        if (strcmp(tabelaSimbolos[i].lexema, lexema) == 0) return i;
    }
    return -1;
}

int inserirSimbolo(const char *lexema, TipoToken tipo, Categoria categoria, int linha) {
    Simbolo *s;

    if (qtdSimbolos >= MAX_SIMBOLOS) return -1;

    s = &tabelaSimbolos[qtdSimbolos];
    strncpy(s->lexema, lexema, sizeof(s->lexema) - 1);
    s->lexema[sizeof(s->lexema) - 1] = '\0';
    s->tipo = tipo;
    s->categoria = categoria;
    s->linha = linha;
    return qtdSimbolos++;
}

const char *nomeCategoria(Categoria categoria) {
    switch (categoria) {
        case CAT_RESERVADA:     return "palavra reservada";
        case CAT_OPERADOR:      return "operador";
        case CAT_PONTUACAO:     return "pontuacao";
        case CAT_IDENTIFICADOR: return "identificador";
    }
    return "?";
}

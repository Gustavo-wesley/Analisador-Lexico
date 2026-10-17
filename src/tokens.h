#ifndef TOKENS_H
#define TOKENS_H

/* Todos os tipos de token da linguagem Pasquick (Anexo I) */
typedef enum {
    /* palavras reservadas */
    TK_PROGRAM,
    TK_BEGIN,
    TK_END,
    TK_CONST,
    TK_VAR,
    TK_INTEGER,
    TK_REAL,
    TK_CHAR,
    TK_STRING,
    TK_PROCEDURE,
    TK_FUNCTION,
    TK_IF,
    TK_THEN,
    TK_ELSE,
    TK_WHILE,
    TK_DO,
    TK_REPEAT,
    TK_UNTIL,
    TK_BREAK,
    TK_CONTINUE,
    TK_E,
    TK_OU,
    TK_FOR,          /* reservada agora, usada na analise sintatica */
    TK_TO,

    /* identificadores e constantes */
    TK_ID,
    TK_NUM,
    TK_LITERAL,

    /* operadores */
    TK_ATRIB,        /* :=  */
    TK_MAIS,         /* +   */
    TK_MENOS,        /* -   */
    TK_MULT,         /* *   */
    TK_DIV,          /* /   */
    TK_IGUAL,        /* =   */
    TK_DIFERENTE,    /* <>  */
    TK_MENOR,        /* <   */
    TK_MENOR_IGUAL,  /* <=  */
    TK_MAIOR,        /* >   */
    TK_MAIOR_IGUAL,  /* >=  */

    /* pontuacao */
    TK_PONTO_VIRGULA, /* ; */
    TK_VIRGULA,       /* , */
    TK_PONTO,         /* . */
    TK_DOIS_PONTOS,   /* : */
    TK_ABRE_PAR,      /* ( */
    TK_FECHA_PAR,     /* ) */
    TK_ABRE_COL,      /* [ */
    TK_FECHA_COL,     /* ] */

    TK_EOF,
    TK_QTD_TIPOS
} TipoToken;

const char *nomeToken(TipoToken tipo);

#endif

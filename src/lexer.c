#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"
#include "simbolos.h"
#include "erros.h"

Token tokens[MAX_TOKENS];
int qtdTokens = 0;

/* Estados do automato finito deterministico (ver diagrama do AFD no relatorio) */
typedef enum {
    Q0,               /* estado inicial                         */
    Q_ID,             /* lendo identificador ou palavra reservada */
    Q_NUM,            /* lendo parte inteira de um numero        */
    Q_NUM_PONTO,      /* leu digitos seguidos de '.'             */
    Q_NUM_REAL,       /* lendo parte decimal                     */
    Q_NUM_ERRO,       /* numero colado com letra (ex: 12abc)     */
    Q_LITERAL,        /* dentro de '...'                         */
    Q_BARRA,          /* leu '/'                                 */
    Q_COMENT_LINHA,   /* dentro de // ...                        */
    Q_COMENT_CHAVE,   /* dentro de { ... }                       */
    Q_ABRE_PAR,       /* leu '('                                 */
    Q_COMENT_PAR,     /* dentro de (* ... *)                     */
    Q_COMENT_PAR_AST, /* dentro de (* ... e acabou de ler '*'    */
    Q_DOIS_PONTOS,    /* leu ':'                                 */
    Q_MENOR,          /* leu '<'                                 */
    Q_MAIOR           /* leu '>'                                 */
} Estado;

static const char *fonte;
static int pos, linha, coluna;

/* onde comecou o lexema atual */
static int iniPos, iniLinha, iniColuna;

/* ---------- funcoes auxiliares ---------- */

static char atual(void) {
    return fonte[pos];
}

/* Bytes de continuacao do UTF-8 (acentos) nao contam como nova coluna */
static int ehContinuacaoUtf8(char c) {
    return ((unsigned char)c & 0xC0) == 0x80;
}

static void avancar(void) {
    char c = fonte[pos];

    if (c == '\0') return;
    if (c == '\n') {
        linha++;
        coluna = 1;
    } else if (!ehContinuacaoUtf8(c)) {
        coluna++;
    }
    pos++;
}

static int ehLetra(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int ehDigito(char c) {
    return c >= '0' && c <= '9';
}

/* Tamanho do lexema atual em colunas (so faz sentido na mesma linha) */
static int tamanhoAtual(void) {
    if (linha != iniLinha) return 1;
    return coluna - iniColuna;
}

static Token *emitir(TipoToken tipo) {
    static Token descarte;
    Token *t;
    int tam = pos - iniPos;

    if (qtdTokens >= MAX_TOKENS) return &descarte;

    t = &tokens[qtdTokens++];
    if (tam > MAX_LEXEMA - 1) tam = MAX_LEXEMA - 1;
    memcpy(t->lexema, fonte + iniPos, tam);
    t->lexema[tam] = '\0';

    t->tipo = tipo;
    t->linha = iniLinha;
    t->coluna = iniColuna;
    t->tamanho = tamanhoAtual();
    t->inicio = iniPos;
    t->bytes = pos - iniPos;
    t->valor = 0;
    t->ehReal = 0;
    t->indiceTS = -1;

    /* operadores e pontuacao ja estao na tabela de simbolos */
    if (tipo != TK_NUM && tipo != TK_LITERAL && tipo != TK_EOF) {
        t->indiceTS = buscarSimbolo(t->lexema);
    }
    return t;
}

static void finalizarIdentificador(void) {
    char maiusculo[MAX_TAM_ID + 1];
    int tam = pos - iniPos;
    int i, indice;
    Token *t;

    if (tam > MAX_TAM_ID) {
        /* recuperacao: descarta o identificador inteiro e continua */
        adicionarErro(iniLinha, iniColuna, tamanhoAtual(),
                      "Identificador com mais de 15 caracteres");
        return;
    }

    /* a linguagem nao diferencia maiusculas de minusculas */
    for (i = 0; i < tam; i++) {
        maiusculo[i] = (char)toupper((unsigned char)fonte[iniPos + i]);
    }
    maiusculo[tam] = '\0';

    indice = buscarSimbolo(maiusculo);
    if (indice == -1) {
        indice = inserirSimbolo(maiusculo, TK_ID, CAT_IDENTIFICADOR, iniLinha);
    }

    if (indice == -1) {
        t = emitir(TK_ID);   /* tabela cheia: segue sem indice */
    } else {
        t = emitir(tabelaSimbolos[indice].tipo);   /* TK_ID ou a palavra reservada */
    }
    t->indiceTS = indice;
}

static void emitirNumero(void) {
    Token *t = emitir(TK_NUM);
    t->valor = atof(t->lexema);
    t->ehReal = strchr(t->lexema, '.') != NULL;
}

static void erroCaractereInvalido(void) {
    char msg[128];
    char c = atual();

    if ((unsigned char)c >= 0x80) {
        sprintf(msg, "Caractere invalido (acento ou simbolo fora de string/comentario)");
    } else {
        sprintf(msg, "Caractere invalido '%c'", c);
    }

    /* recuperacao (modo panico): descarta o caractere e continua */
    avancar();
    while (ehContinuacaoUtf8(atual())) avancar();

    adicionarErro(iniLinha, iniColuna, 1, msg);
}

/* ---------- automato ---------- */

void analisarFonte(const char *texto) {
    Estado estado = Q0;
    char c;

    fonte = texto;
    pos = 0;
    linha = 1;
    coluna = 1;
    qtdTokens = 0;
    limparErros();
    iniciarTabelaSimbolos();

    while (1) {
        c = atual();

        switch (estado) {
        case Q0:
            iniPos = pos;
            iniLinha = linha;
            iniColuna = coluna;

            if (c == '\0') {
                emitir(TK_EOF);
                return;
            }
            else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') avancar();
            else if (ehLetra(c))  { avancar(); estado = Q_ID; }
            else if (ehDigito(c)) { avancar(); estado = Q_NUM; }
            else if (c == '\'')   { avancar(); estado = Q_LITERAL; }
            else if (c == '/')    { avancar(); estado = Q_BARRA; }
            else if (c == '{')    { avancar(); estado = Q_COMENT_CHAVE; }
            else if (c == '(')    { avancar(); estado = Q_ABRE_PAR; }
            else if (c == ':')    { avancar(); estado = Q_DOIS_PONTOS; }
            else if (c == '<')    { avancar(); estado = Q_MENOR; }
            else if (c == '>')    { avancar(); estado = Q_MAIOR; }
            else if (c == '+')    { avancar(); emitir(TK_MAIS); }
            else if (c == '-')    { avancar(); emitir(TK_MENOS); }
            else if (c == '*')    { avancar(); emitir(TK_MULT); }
            else if (c == '=')    { avancar(); emitir(TK_IGUAL); }
            else if (c == ';')    { avancar(); emitir(TK_PONTO_VIRGULA); }
            else if (c == ',')    { avancar(); emitir(TK_VIRGULA); }
            else if (c == '.')    { avancar(); emitir(TK_PONTO); }
            else if (c == ')')    { avancar(); emitir(TK_FECHA_PAR); }
            else if (c == '[')    { avancar(); emitir(TK_ABRE_COL); }
            else if (c == ']')    { avancar(); emitir(TK_FECHA_COL); }
            else erroCaractereInvalido();
            break;

        case Q_ID:
            if (ehLetra(c) || ehDigito(c) || c == '_') avancar();
            else { finalizarIdentificador(); estado = Q0; }
            break;

        case Q_NUM:
            if (ehDigito(c)) avancar();
            else if (c == '.') { avancar(); estado = Q_NUM_PONTO; }
            else if (ehLetra(c) || c == '_') { avancar(); estado = Q_NUM_ERRO; }
            else { emitirNumero(); estado = Q0; }
            break;

        case Q_NUM_PONTO:
            if (ehDigito(c)) { avancar(); estado = Q_NUM_REAL; }
            else {
                adicionarErro(iniLinha, iniColuna, tamanhoAtual(),
                              "Numero mal formado: falta digito depois do ponto");
                estado = Q0;
            }
            break;

        case Q_NUM_REAL:
            if (ehDigito(c)) avancar();
            else if (ehLetra(c) || c == '_' || c == '.') { avancar(); estado = Q_NUM_ERRO; }
            else { emitirNumero(); estado = Q0; }
            break;

        case Q_NUM_ERRO:
            /* recuperacao: consome o resto da "palavra" e descarta */
            if (ehLetra(c) || ehDigito(c) || c == '_' || c == '.') avancar();
            else {
                adicionarErro(iniLinha, iniColuna, tamanhoAtual(), "Numero mal formado");
                estado = Q0;
            }
            break;

        case Q_LITERAL:
            if (c == '\'') {
                avancar();
                emitir(TK_LITERAL);
                estado = Q0;
            }
            else if (c == '\n' || c == '\0') {
                /* recuperacao: o literal termina no fim da linha */
                adicionarErro(iniLinha, iniColuna, tamanhoAtual(),
                              "Literal nao fechado (falta o ')");
                estado = Q0;
            }
            else avancar();
            break;

        case Q_BARRA:
            if (c == '/') { avancar(); estado = Q_COMENT_LINHA; }
            else { emitir(TK_DIV); estado = Q0; }
            break;

        case Q_COMENT_LINHA:
            if (c == '\n' || c == '\0') estado = Q0;
            else avancar();
            break;

        case Q_COMENT_CHAVE:
            if (c == '}') { avancar(); estado = Q0; }
            else if (c == '\0') {
                adicionarErro(iniLinha, iniColuna, 1, "Comentario { nao foi fechado");
                estado = Q0;
            }
            else avancar();
            break;

        case Q_ABRE_PAR:
            if (c == '*') { avancar(); estado = Q_COMENT_PAR; }
            else { emitir(TK_ABRE_PAR); estado = Q0; }
            break;

        case Q_COMENT_PAR:
            if (c == '*') { avancar(); estado = Q_COMENT_PAR_AST; }
            else if (c == '\0') {
                adicionarErro(iniLinha, iniColuna, 2, "Comentario (* nao foi fechado");
                estado = Q0;
            }
            else avancar();
            break;

        case Q_COMENT_PAR_AST:
            if (c == ')') { avancar(); estado = Q0; }
            else if (c == '*') avancar();
            else if (c == '\0') {
                adicionarErro(iniLinha, iniColuna, 2, "Comentario (* nao foi fechado");
                estado = Q0;
            }
            else { avancar(); estado = Q_COMENT_PAR; }
            break;

        case Q_DOIS_PONTOS:
            if (c == '=') { avancar(); emitir(TK_ATRIB); }
            else emitir(TK_DOIS_PONTOS);
            estado = Q0;
            break;

        case Q_MENOR:
            if (c == '=')      { avancar(); emitir(TK_MENOR_IGUAL); }
            else if (c == '>') { avancar(); emitir(TK_DIFERENTE); }
            else emitir(TK_MENOR);
            estado = Q0;
            break;

        case Q_MAIOR:
            if (c == '=') { avancar(); emitir(TK_MAIOR_IGUAL); }
            else emitir(TK_MAIOR);
            estado = Q0;
            break;
        }
    }
}

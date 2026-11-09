#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "arvore.h"
#include "lexer.h"
#include "simbolos.h"

/* Os nos ficam num vetor fixo, como os tokens: nao precisa de free() e
   a arvore inteira e descartada a cada nova analise. */
static No nos[MAX_NOS];
static int qtdNos = 0;
static No noDescarte;   /* devolvido quando o vetor enche, para nao travar */

No *raiz = NULL;

LinhaArvore linhasArvore[MAX_LINHAS_ARVORE];
int qtdLinhasArvore = 0;

void limparArvore(void) {
    qtdNos = 0;
    raiz = NULL;
    qtdLinhasArvore = 0;
}

No *novoNo(TipoNo tipo, int tk) {
    No *n = qtdNos < MAX_NOS ? &nos[qtdNos++] : &noDescarte;

    memset(n, 0, sizeof(*n));
    n->tipo = tipo;
    n->tk = tk;
    n->declaracao = -1;
    return n;
}

const char *nomeId(int tk) {
    Token *t = &tokens[tk];
    if (t->indiceTS >= 0) return tabelaSimbolos[t->indiceTS].lexema;
    return t->lexema;
}

TipoDado tipoSimples(TipoBase base) {
    TipoDado t;
    t.base = base;
    t.vetor = 0;
    t.tamanho = 0;
    return t;
}

const char *nomeTipo(TipoDado t) {
    /* varios buffers para poder usar mais de um nomeTipo() no mesmo sprintf */
    static char buffers[4][32];
    static int proximo = 0;
    char *b = buffers[proximo];
    const char *base = "?";

    proximo = (proximo + 1) % 4;

    switch (t.base) {
        case T_NENHUM:  base = "-";       break;
        case T_ERRO:    base = "?";       break;
        case T_INTEGER: base = "integer"; break;
        case T_REAL:    base = "real";    break;
        case T_CHAR:    base = "char";    break;
        case T_STRING:  base = "string";  break;
        case T_LOGICO:  base = "logico";  break;
    }
    if (t.vetor) sprintf(b, "%s[%d]", base, t.tamanho);
    else sprintf(b, "%s", base);
    return b;
}

/* ---------- arvore em texto ---------- */

static void escreverLinha(int nivel, int tk, const char *formato, ...) {
    LinhaArvore *l;
    va_list args;
    int recuo = nivel * 2;

    if (qtdLinhasArvore >= MAX_LINHAS_ARVORE) return;
    l = &linhasArvore[qtdLinhasArvore++];

    if (recuo > 60) recuo = 60;
    memset(l->texto, ' ', recuo);
    va_start(args, formato);
    vsnprintf(l->texto + recuo, sizeof(l->texto) - recuo, formato, args);
    va_end(args);

    l->linha = tk >= 0 ? tokens[tk].linha : 0;
    l->coluna = tk >= 0 ? tokens[tk].coluna : 0;
}

/* tipo calculado pelo semantico, mostrado ao lado das expressoes */
static const char *anotacao(No *n) {
    static char b[48];
    if (n->tipoDado.base == T_NENHUM) return "";
    sprintf(b, "   <%s>", nomeTipo(n->tipoDado));
    return b;
}

static void imprimirNo(No *n, int nivel);

static void imprimirLista(No *n, int nivel) {
    for (; n != NULL; n = n->prox) imprimirNo(n, nivel);
}

static void imprimirNo(No *n, int nivel) {
    if (n == NULL) return;

    switch (n->tipo) {
    case NO_PROGRAMA:
        escreverLinha(nivel, n->tk, "programa %s", nomeId(n->tk));
        imprimirLista(n->filho[0], nivel + 1);
        imprimirNo(n->filho[1], nivel + 1);
        break;

    case NO_VAR:
        escreverLinha(nivel, n->tk, "var %s : %s", nomeId(n->tk), nomeTipo(n->tipoDeclarado));
        break;

    case NO_CONST: {
        TipoDado t = n->tipoDeclarado.base != T_NENHUM ? n->tipoDeclarado : n->tipoDado;
        if (t.base == T_NENHUM)   /* tipo omitido: so o semantico descobre */
            escreverLinha(nivel, n->tk, "const %s", nomeId(n->tk));
        else
            escreverLinha(nivel, n->tk, "const %s : %s", nomeId(n->tk), nomeTipo(t));
        imprimirNo(n->filho[0], nivel + 1);
        break;
    }

    case NO_PROCEDIMENTO:
    case NO_FUNCAO:
        if (n->tipo == NO_FUNCAO)
            escreverLinha(nivel, n->tk, "function %s : %s", nomeId(n->tk), nomeTipo(n->tipoDeclarado));
        else
            escreverLinha(nivel, n->tk, "procedure %s", nomeId(n->tk));
        imprimirLista(n->filho[0], nivel + 1);
        imprimirLista(n->filho[1], nivel + 1);
        imprimirNo(n->filho[2], nivel + 1);
        break;

    case NO_PARAMETRO:
        escreverLinha(nivel, n->tk, "param %s : %s", nomeId(n->tk), nomeTipo(n->tipoDeclarado));
        break;

    case NO_BLOCO:
        escreverLinha(nivel, n->tk, "bloco");
        imprimirLista(n->filho[0], nivel + 1);
        break;

    case NO_ATRIB:
        escreverLinha(nivel, n->tk, ":=");
        imprimirNo(n->filho[0], nivel + 1);
        imprimirNo(n->filho[1], nivel + 1);
        break;

    case NO_CHAMADA:
        escreverLinha(nivel, n->tk, "chamada %s%s", nomeId(n->tk), anotacao(n));
        imprimirLista(n->filho[0], nivel + 1);
        break;

    case NO_IF:
        escreverLinha(nivel, n->tk, "if");
        imprimirNo(n->filho[0], nivel + 1);
        escreverLinha(nivel + 1, n->tk, "then");
        imprimirNo(n->filho[1], nivel + 2);
        if (n->filho[2] != NULL) {
            escreverLinha(nivel + 1, n->tk, "else");
            imprimirNo(n->filho[2], nivel + 2);
        }
        break;

    case NO_WHILE:
        escreverLinha(nivel, n->tk, "while");
        imprimirNo(n->filho[0], nivel + 1);
        escreverLinha(nivel + 1, n->tk, "do");
        imprimirNo(n->filho[1], nivel + 2);
        break;

    case NO_REPEAT:
        escreverLinha(nivel, n->tk, "repeat");
        imprimirLista(n->filho[0], nivel + 1);
        escreverLinha(nivel + 1, n->tk, "until");
        imprimirNo(n->filho[1], nivel + 2);
        break;

    case NO_FOR:
        escreverLinha(nivel, n->tk, "for");
        imprimirNo(n->filho[0], nivel + 1);
        imprimirNo(n->filho[1], nivel + 1);
        escreverLinha(nivel + 1, n->tk, "to");
        imprimirNo(n->filho[2], nivel + 2);
        escreverLinha(nivel + 1, n->tk, "do");
        imprimirNo(n->filho[3], nivel + 2);
        break;

    case NO_BREAK:
        escreverLinha(nivel, n->tk, "break");
        break;

    case NO_CONTINUE:
        escreverLinha(nivel, n->tk, "continue");
        break;

    case NO_BINARIO:
        escreverLinha(nivel, n->tk, "%s%s", tokens[n->tk].lexema, anotacao(n));
        imprimirNo(n->filho[0], nivel + 1);
        imprimirNo(n->filho[1], nivel + 1);
        break;

    case NO_UNARIO:
        escreverLinha(nivel, n->tk, "%s (unario)%s", tokens[n->tk].lexema, anotacao(n));
        imprimirNo(n->filho[0], nivel + 1);
        break;

    case NO_NUM:
        escreverLinha(nivel, n->tk, "num %s%s", tokens[n->tk].lexema, anotacao(n));
        break;

    case NO_LITERAL:
        escreverLinha(nivel, n->tk, "literal %s%s", tokens[n->tk].lexema, anotacao(n));
        break;

    case NO_ID:
        escreverLinha(nivel, n->tk, "id %s%s", nomeId(n->tk), anotacao(n));
        break;

    case NO_INDICE:
        escreverLinha(nivel, n->tk, "indice %s%s", nomeId(n->tk), anotacao(n));
        imprimirNo(n->filho[0], nivel + 1);
        break;

    case NO_QTD_TIPOS:
        break;
    }
}

void gerarLinhasArvore(void) {
    qtdLinhasArvore = 0;
    imprimirNo(raiz, 0);
}

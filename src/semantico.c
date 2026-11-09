/* Analisador semantico do Pasquick.

   Percorre a arvore gerada pelo sintatico e confere o que a gramatica
   sozinha nao consegue garantir:

   Declaracoes e escopo
     - todo nome usado precisa ter sido declarado antes (no escopo atual ou
       em um escopo que o envolve);
     - o mesmo nome nao pode ser declarado duas vezes no mesmo escopo;
     - parametros e variaveis locais de uma sub-rotina so existem dentro dela.

   Tipos
     - atribuicao: o tipo da expressao precisa ser compativel com o destino
       (integer -> real e char -> string sao aceitos, o contrario nao);
     - operadores aritmeticos so com numeros ('+' tambem concatena textos);
     - comparacoes entre numeros ou entre textos; E/OU so com comparacoes;
     - condicao de if/while/until precisa ser logica (uma comparacao);
     - indice de vetor/string precisa ser integer.

   Uso correto de cada categoria
     - constante, procedimento e programa nao recebem valor;
     - o nome da funcao so recebe valor dentro dela (valor de retorno), e
       toda funcao precisa receber um valor de retorno;
     - procedimento nao pode aparecer em expressao;
     - chamada com a quantidade e os tipos certos de argumentos;
     - variavel de controle do for e os limites precisam ser integer;
     - break e continue so dentro de laco.

   write, writeln, read e readln ja vem declarados (procedimentos com
   quantidade variavel de argumentos). */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "semantico.h"
#include "lexer.h"
#include "erros.h"

Declaracao declaracoes[MAX_DECLARACOES];
int qtdDeclaracoes = 0;

#define MAX_NIVEIS 64

/* Pilha de escopos: visiveis[] guarda os indices das declaracoes que podem
   ser usadas agora. Ao sair de um escopo, as dele sao desempilhadas, mas
   continuam em declaracoes[] para aparecer na tabela. */
static int visiveis[MAX_DECLARACOES];
static int qtdVisiveis;

static int nivel;
static int inicioEscopo[MAX_NIVEIS];
static int subRotinaDoNivel[MAX_NIVEIS];   /* declaracao da sub-rotina dona do escopo (-1 = global) */
static int retornoAtribuido[MAX_NIVEIS];   /* a funcao do nivel ja recebeu valor de retorno */
static char nomeEscopo[MAX_NIVEIS][32];

static int profundidadeLaco;   /* quantos while/repeat/for envolvem o comando atual */

/* ---------- mensagens de erro ---------- */

static int primeiroToken(No *e) {
    while (e->tipo == NO_BINARIO && e->filho[0] != NULL) e = e->filho[0];
    return e->tk;
}

static int ultimoToken(No *e) {
    No *a;

    switch (e->tipo) {
    case NO_BINARIO:
        return e->filho[1] ? ultimoToken(e->filho[1]) : e->tk;
    case NO_UNARIO:
        return e->filho[0] ? ultimoToken(e->filho[0]) : e->tk;
    case NO_INDICE:
        return e->filho[0] ? ultimoToken(e->filho[0]) + 1 : e->tk;   /* +1 = o ']' */
    case NO_CHAMADA:
        a = e->filho[0];
        if (a == NULL) return tokens[e->tk + 1].tipo == TK_ABRE_PAR ? e->tk + 2 : e->tk;
        while (a->prox != NULL) a = a->prox;
        return ultimoToken(a) + 1;   /* +1 = o ')' */
    default:
        return e->tk;
    }
}

static void registrar(int tkIni, int tkFim, const char *formato, va_list args) {
    char msg[128];
    Token *ini = &tokens[tkIni];
    Token *fim = &tokens[tkFim < qtdTokens ? tkFim : tkIni];
    int tamanho = ini->tamanho;

    vsnprintf(msg, sizeof(msg), formato, args);
    if (fim->linha == ini->linha && fim->coluna >= ini->coluna)
        tamanho = fim->coluna + fim->tamanho - ini->coluna;

    adicionarErroFase(FASE_SEMANTICA, ini->linha, ini->coluna, tamanho, msg);
}

/* erro sublinhando um token */
static void erroToken(int tk, const char *formato, ...) {
    va_list args;
    va_start(args, formato);
    registrar(tk, tk, formato, args);
    va_end(args);
}

/* erro sublinhando uma expressao inteira */
static void erroNo(No *e, const char *formato, ...) {
    va_list args;
    va_start(args, formato);
    registrar(primeiroToken(e), ultimoToken(e), formato, args);
    va_end(args);
}

/* nome como o usuario escreveu, para as mensagens */
static const char *lex(int tk) {
    return tokens[tk].lexema;
}

/* ---------- escopos e tabela de declaracoes ---------- */

static void abrirEscopo(const char *nome, int subRotina) {
    if (nivel + 1 >= MAX_NIVEIS) return;
    nivel++;
    inicioEscopo[nivel] = qtdVisiveis;
    subRotinaDoNivel[nivel] = subRotina;
    retornoAtribuido[nivel] = 0;
    strncpy(nomeEscopo[nivel], nome, 31);
    nomeEscopo[nivel][31] = '\0';
}

static void fecharEscopo(void) {
    if (nivel == 0) return;
    qtdVisiveis = inicioEscopo[nivel];
    nivel--;
}

static int buscarNoEscopoAtual(const char *nome) {
    int i;
    for (i = qtdVisiveis - 1; i >= inicioEscopo[nivel]; i--) {
        if (strcmp(declaracoes[visiveis[i]].nome, nome) == 0) return visiveis[i];
    }
    return -1;
}

/* procura do escopo mais interno para o mais externo */
static int buscar(const char *nome) {
    int i;
    for (i = qtdVisiveis - 1; i >= 0; i--) {
        if (strcmp(declaracoes[visiveis[i]].nome, nome) == 0) return visiveis[i];
    }
    return -1;
}

static int novaDeclaracao(const char *nome, CategoriaDec cat, TipoDado tipo, int linha, int coluna) {
    Declaracao *d;

    if (qtdDeclaracoes >= MAX_DECLARACOES) return -1;
    d = &declaracoes[qtdDeclaracoes];
    memset(d, 0, sizeof(*d));
    strncpy(d->nome, nome, sizeof(d->nome) - 1);
    d->categoria = cat;
    d->tipo = tipo;
    strcpy(d->escopo, nomeEscopo[nivel]);
    d->nivel = nivel;
    d->linha = linha;
    d->coluna = coluna;
    d->primeiroParam = -1;

    visiveis[qtdVisiveis++] = qtdDeclaracoes;
    return qtdDeclaracoes++;
}

/* Declara o identificador do token tk no escopo atual. Retorna o indice ou -1. */
static int declarar(int tk, CategoriaDec cat, TipoDado tipo) {
    const char *nome = nomeId(tk);
    int j = buscarNoEscopoAtual(nome);

    if (j >= 0) {
        if (declaracoes[j].predefinida)
            erroToken(tk, "'%s' ja e um procedimento pre-definido e nao pode ser redeclarado", lex(tk));
        else
            erroToken(tk, "'%s' ja foi declarado neste escopo (linha %d)", lex(tk), declaracoes[j].linha);
        return -1;
    }
    return novaDeclaracao(nome, cat, tipo, tokens[tk].linha, tokens[tk].coluna);
}

static void predefinir(const char *nome) {
    int i = novaDeclaracao(nome, DEC_PROCEDIMENTO, tipoSimples(T_NENHUM), 0, 0);
    if (i < 0) return;
    declaracoes[i].predefinida = 1;
    declaracoes[i].qtdParams = -1;
}

/* nivel da pilha em que estamos dentro da sub-rotina `dec` (-1 se nao estamos) */
static int nivelDentroDe(int dec) {
    int n;
    for (n = nivel; n > 0; n--) {
        if (subRotinaDoNivel[n] == dec) return n;
    }
    return -1;
}

/* ---------- regras de tipos ---------- */

static TipoDado tipoErro(void) {
    return tipoSimples(T_ERRO);
}

static int ehNumerico(TipoDado t) {
    return !t.vetor && (t.base == T_INTEGER || t.base == T_REAL);
}

static int ehTexto(TipoDado t) {
    return !t.vetor && (t.base == T_CHAR || t.base == T_STRING);
}

/* origem pode ser guardada em destino? */
static int atribuivel(TipoDado destino, TipoDado origem) {
    if (destino.base == T_ERRO || origem.base == T_ERRO) return 1;   /* erro ja foi avisado */

    if (destino.vetor || origem.vetor)
        return destino.vetor && origem.vetor && destino.base == origem.base &&
               destino.tamanho == origem.tamanho;

    if (destino.base == T_LOGICO || origem.base == T_LOGICO) return 0;  /* nao existe variavel logica */
    if (destino.base == origem.base) return 1;
    if (destino.base == T_REAL && origem.base == T_INTEGER) return 1;
    if (destino.base == T_STRING && origem.base == T_CHAR) return 1;
    return 0;
}

/* quantidade de caracteres de um literal, sem as aspas (UTF-8 conta 1 por caractere) */
static int tamanhoLiteral(int tk) {
    const char *s = tokens[tk].lexema;
    int i, n = 0;
    int fim = (int)strlen(s) - 1;   /* posicao da aspa de fechamento */

    for (i = 1; i < fim; i++) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) n++;
    }
    return n;
}

static void verificarTipoDeclarado(No *n) {
    if (n->tipoDeclarado.vetor && n->tipoDeclarado.tamanho < 1) {
        erroToken(n->tk, "O tamanho do vetor '%s' deve ser um inteiro maior que zero", lex(n->tk));
        n->tipoDeclarado = tipoErro();
    }
}

/* ---------- expressoes ---------- */

static TipoDado analisarExpressao(No *e);
static void analisarComando(No *n);
static void analisarComandos(No *lista);
static void analisarDeclaracoes(No *lista);

/* expressao formada so por numeros, literais e constantes? */
static int ehConstante(No *e) {
    if (e == NULL) return 1;
    switch (e->tipo) {
        case NO_NUM:
        case NO_LITERAL: return 1;
        case NO_ID:      return e->declaracao >= 0 && declaracoes[e->declaracao].categoria == DEC_CONSTANTE;
        case NO_UNARIO:  return ehConstante(e->filho[0]);
        case NO_BINARIO: return ehConstante(e->filho[0]) && ehConstante(e->filho[1]);
        default:         return 0;
    }
}

/* v[i] ou s[i]: confere o indice e devolve o tipo do elemento */
static TipoDado tipoIndexado(No *n, TipoDado base) {
    TipoDado ti = analisarExpressao(n->filho[0]);

    if (ti.base != T_ERRO && (ti.base != T_INTEGER || ti.vetor))
        erroNo(n->filho[0], "O indice de '%s' deve ser integer, mas e %s", lex(n->tk), nomeTipo(ti));

    if (base.base == T_ERRO) return tipoErro();
    if (base.vetor) return tipoSimples(base.base);
    if (base.base == T_STRING) return tipoSimples(T_CHAR);

    erroToken(n->tk, "'%s' e do tipo %s: so vetor ou string pode ter indice", lex(n->tk), nomeTipo(base));
    return tipoErro();
}

static void verificarArgumentos(No *chamada, Declaracao *d) {
    No *arg;
    int qtd = 0, k;

    for (arg = chamada->filho[0]; arg != NULL; arg = arg->prox) qtd++;

    if (d->predefinida) {
        int leitura = strcmp(d->nome, "READ") == 0 || strcmp(d->nome, "READLN") == 0;

        for (arg = chamada->filho[0], k = 1; arg != NULL; arg = arg->prox, k++) {
            TipoDado t = analisarExpressao(arg);

            if (t.base == T_ERRO) continue;
            if (t.vetor) {
                erroNo(arg, "Argumento %d de '%s': nao da para %s um vetor inteiro, use um indice",
                       k, lex(chamada->tk), leitura ? "ler" : "escrever");
            } else if (t.base == T_LOGICO) {
                erroNo(arg, "Argumento %d de '%s': valor logico nao pode ser escrito", k, lex(chamada->tk));
            } else if (leitura) {
                int ok = (arg->tipo == NO_ID || arg->tipo == NO_INDICE) && arg->declaracao >= 0 &&
                         (declaracoes[arg->declaracao].categoria == DEC_VARIAVEL ||
                          declaracoes[arg->declaracao].categoria == DEC_PARAMETRO);
                if (!ok) erroNo(arg, "Argumento %d de '%s' precisa ser uma variavel", k, lex(chamada->tk));
            }
        }
        return;
    }

    if (qtd != d->qtdParams) {
        erroToken(chamada->tk, "'%s' espera %d argumento(s), mas recebeu %d",
                  lex(chamada->tk), d->qtdParams, qtd);
        for (arg = chamada->filho[0]; arg != NULL; arg = arg->prox) analisarExpressao(arg);
        return;
    }

    for (arg = chamada->filho[0], k = 0; arg != NULL; arg = arg->prox, k++) {
        TipoDado ta = analisarExpressao(arg);
        TipoDado tp = declaracoes[d->primeiroParam + k].tipo;

        if (!atribuivel(tp, ta))
            erroNo(arg, "Argumento %d de '%s': esperado %s, encontrado %s",
                   k + 1, lex(chamada->tk), nomeTipo(tp), nomeTipo(ta));
    }
}

/* chamada com parenteses, como comando (emExpressao = 0) ou dentro de expressao */
static TipoDado analisarChamada(No *n, int emExpressao) {
    int i = buscar(nomeId(n->tk));
    No *arg;
    Declaracao *d;

    if (i < 0) {
        erroToken(n->tk, "'%s' nao foi declarado", lex(n->tk));
        for (arg = n->filho[0]; arg != NULL; arg = arg->prox) analisarExpressao(arg);
        return tipoErro();
    }

    d = &declaracoes[i];
    d->usada = 1;
    n->declaracao = i;

    if (d->categoria != DEC_PROCEDIMENTO && d->categoria != DEC_FUNCAO) {
        erroToken(n->tk, "'%s' e %s, nao procedimento nem funcao", lex(n->tk), nomeCategoriaDec(d->categoria));
        for (arg = n->filho[0]; arg != NULL; arg = arg->prox) analisarExpressao(arg);
        return tipoErro();
    }

    verificarArgumentos(n, d);

    if (emExpressao && d->categoria == DEC_PROCEDIMENTO) {
        erroToken(n->tk, "'%s' e um procedimento: nao devolve valor e nao pode ser usado em expressao",
                  lex(n->tk));
        return tipoErro();
    }
    return d->categoria == DEC_FUNCAO ? d->tipo : tipoSimples(T_NENHUM);
}

/* identificador sozinho dentro de uma expressao */
static TipoDado analisarUsoId(No *e) {
    int i = buscar(nomeId(e->tk));
    Declaracao *d;

    if (i < 0) {
        erroToken(e->tk, "'%s' nao foi declarado", lex(e->tk));
        return tipoErro();
    }

    d = &declaracoes[i];
    d->usada = 1;
    e->declaracao = i;

    switch (d->categoria) {
    case DEC_VARIAVEL:
    case DEC_PARAMETRO:
    case DEC_CONSTANTE:
        return d->tipo;

    case DEC_FUNCAO:
        /* funcao sem parenteses = chamada sem argumentos (como em Pascal) */
        if (d->qtdParams != 0)
            erroToken(e->tk, "'%s' espera %d argumento(s), mas recebeu 0", lex(e->tk), d->qtdParams);
        return d->tipo;

    case DEC_PROCEDIMENTO:
        erroToken(e->tk, "'%s' e um procedimento: nao devolve valor e nao pode ser usado em expressao",
                  lex(e->tk));
        return tipoErro();

    case DEC_PROGRAMA:
        erroToken(e->tk, "'%s' e o nome do programa e nao pode ser usado em expressao", lex(e->tk));
        return tipoErro();
    }
    return tipoErro();
}

/* v[i] dentro de uma expressao */
static TipoDado analisarIndice(No *e) {
    int i = buscar(nomeId(e->tk));
    Declaracao *d;

    if (i < 0) {
        erroToken(e->tk, "'%s' nao foi declarado", lex(e->tk));
        analisarExpressao(e->filho[0]);
        return tipoErro();
    }

    d = &declaracoes[i];
    d->usada = 1;
    e->declaracao = i;

    if (d->categoria != DEC_VARIAVEL && d->categoria != DEC_PARAMETRO && d->categoria != DEC_CONSTANTE) {
        erroToken(e->tk, "'%s' e %s e nao pode ter indice", lex(e->tk), nomeCategoriaDec(d->categoria));
        analisarExpressao(e->filho[0]);
        return tipoErro();
    }
    return tipoIndexado(e, d->tipo);
}

static TipoDado analisarBinario(No *e) {
    TipoDado a = analisarExpressao(e->filho[0]);
    TipoDado b = analisarExpressao(e->filho[1]);
    const char *op = tokens[e->tk].lexema;

    if (a.base == T_ERRO || b.base == T_ERRO) return tipoErro();

    switch (e->op) {
    case TK_MAIS:
    case TK_MENOS:
    case TK_MULT:
    case TK_DIV:
        /* '+' tambem junta textos (concatenacao) */
        if (e->op == TK_MAIS && ehTexto(a) && ehTexto(b)) return tipoSimples(T_STRING);
        if (ehNumerico(a) && ehNumerico(b)) {
            /* integer com integer da integer (inclusive '/', ja que nao existe div) */
            if (a.base == T_INTEGER && b.base == T_INTEGER) return tipoSimples(T_INTEGER);
            return tipoSimples(T_REAL);
        }
        break;

    case TK_IGUAL:
    case TK_DIFERENTE:
    case TK_MENOR:
    case TK_MENOR_IGUAL:
    case TK_MAIOR:
    case TK_MAIOR_IGUAL:
        if ((ehNumerico(a) && ehNumerico(b)) || (ehTexto(a) && ehTexto(b)))
            return tipoSimples(T_LOGICO);
        /* '=' e '<>' tambem comparam duas comparacoes: (a < b) = (c < d) */
        if ((e->op == TK_IGUAL || e->op == TK_DIFERENTE) && !a.vetor && !b.vetor &&
            a.base == T_LOGICO && b.base == T_LOGICO)
            return tipoSimples(T_LOGICO);
        erroNo(e, "Nao da para comparar %s com %s usando '%s'", nomeTipo(a), nomeTipo(b), op);
        return tipoErro();

    case TK_E:
    case TK_OU:
        if (!a.vetor && !b.vetor && a.base == T_LOGICO && b.base == T_LOGICO)
            return tipoSimples(T_LOGICO);
        erroNo(e, "O operador '%s' precisa de comparacoes dos dois lados, mas recebeu %s e %s",
               op, nomeTipo(a), nomeTipo(b));
        return tipoErro();

    default:
        break;
    }

    erroNo(e, "O operador '%s' nao se aplica a %s e %s", op, nomeTipo(a), nomeTipo(b));
    return tipoErro();
}

static TipoDado analisarExpressao(No *e) {
    TipoDado t;

    if (e == NULL) return tipoErro();

    switch (e->tipo) {
    case NO_NUM:
        t = tipoSimples(tokens[e->tk].ehReal ? T_REAL : T_INTEGER);
        break;

    case NO_LITERAL:
        t = tipoSimples(tamanhoLiteral(e->tk) == 1 ? T_CHAR : T_STRING);
        break;

    case NO_ID:      t = analisarUsoId(e); break;
    case NO_INDICE:  t = analisarIndice(e); break;
    case NO_CHAMADA: t = analisarChamada(e, 1); break;
    case NO_BINARIO: t = analisarBinario(e); break;

    case NO_UNARIO:
        t = analisarExpressao(e->filho[0]);
        if (t.base != T_ERRO && !ehNumerico(t)) {
            erroNo(e, "O '%s' unario so se aplica a numeros, mas o operando e %s",
                   tokens[e->tk].lexema, nomeTipo(t));
            t = tipoErro();
        }
        break;

    default:
        t = tipoErro();
        break;
    }

    e->tipoDado = t;
    return t;
}

/* condicao de if, while e until */
static void verificarCondicao(No *e, const char *comando) {
    TipoDado t = analisarExpressao(e);

    if (e != NULL && t.base != T_ERRO && (t.base != T_LOGICO || t.vetor))
        erroNo(e, "A condicao do '%s' precisa ser uma comparacao, mas e do tipo %s", comando, nomeTipo(t));
}

/* ---------- comandos ---------- */

/* lado esquerdo do := (x ou v[i]). Devolve o tipo que ele aceita. */
static TipoDado analisarDestino(No *dest) {
    int i = buscar(nomeId(dest->tk));
    Declaracao *d;
    TipoDado t;
    int n;

    if (i < 0) {
        erroToken(dest->tk, "'%s' nao foi declarado", lex(dest->tk));
        if (dest->tipo == NO_INDICE) analisarExpressao(dest->filho[0]);
        return tipoErro();
    }

    d = &declaracoes[i];
    d->usada = 1;
    dest->declaracao = i;

    switch (d->categoria) {
    case DEC_VARIAVEL:
    case DEC_PARAMETRO:
        t = d->tipo;
        break;

    case DEC_FUNCAO:
        n = nivelDentroDe(i);
        if (n < 0) {
            erroToken(dest->tk, "So da para atribuir a '%s' dentro da propria funcao (valor de retorno)",
                      lex(dest->tk));
            return tipoErro();
        }
        if (dest->tipo == NO_INDICE) {
            erroToken(dest->tk, "O retorno da funcao '%s' nao pode ter indice", lex(dest->tk));
            return tipoErro();
        }
        retornoAtribuido[n] = 1;
        t = d->tipo;
        break;

    default:
        erroToken(dest->tk, "'%s' e %s e nao pode receber valor", lex(dest->tk), nomeCategoriaDec(d->categoria));
        if (dest->tipo == NO_INDICE) analisarExpressao(dest->filho[0]);
        return tipoErro();
    }

    if (dest->tipo == NO_INDICE) t = tipoIndexado(dest, t);
    dest->tipoDado = t;
    return t;
}

static void analisarAtribuicao(No *n) {
    TipoDado destino = analisarDestino(n->filho[0]);
    TipoDado valor = analisarExpressao(n->filho[1]);

    if (n->filho[1] != NULL && !atribuivel(destino, valor))
        erroNo(n->filho[1], "Tipos incompativeis: '%s' e %s, mas a expressao e %s",
               lex(n->filho[0]->tk), nomeTipo(destino), nomeTipo(valor));
}

static void analisarFor(No *n) {
    No *v = n->filho[0];
    TipoDado t;

    if (v != NULL) {
        int i = buscar(nomeId(v->tk));

        if (i < 0) {
            erroToken(v->tk, "'%s' nao foi declarado", lex(v->tk));
        } else {
            Declaracao *d = &declaracoes[i];
            d->usada = 1;
            v->declaracao = i;
            v->tipoDado = d->tipo;

            if (d->categoria != DEC_VARIAVEL && d->categoria != DEC_PARAMETRO)
                erroToken(v->tk, "A variavel de controle do for precisa ser uma variavel, e '%s' e %s",
                          lex(v->tk), nomeCategoriaDec(d->categoria));
            else if (d->tipo.base != T_INTEGER || d->tipo.vetor)
                erroToken(v->tk, "A variavel de controle do for precisa ser integer, mas '%s' e %s",
                          lex(v->tk), nomeTipo(d->tipo));
        }
    }

    t = analisarExpressao(n->filho[1]);
    if (t.base != T_ERRO && (t.base != T_INTEGER || t.vetor))
        erroNo(n->filho[1], "O valor inicial do for precisa ser integer, mas e %s", nomeTipo(t));

    t = analisarExpressao(n->filho[2]);
    if (t.base != T_ERRO && (t.base != T_INTEGER || t.vetor))
        erroNo(n->filho[2], "O valor final do for precisa ser integer, mas e %s", nomeTipo(t));

    profundidadeLaco++;
    analisarComando(n->filho[3]);
    profundidadeLaco--;
}

static void analisarComando(No *n) {
    if (n == NULL) return;   /* comando vazio */

    switch (n->tipo) {
    case NO_BLOCO:
        analisarComandos(n->filho[0]);
        break;

    case NO_ATRIB:
        analisarAtribuicao(n);
        break;

    case NO_CHAMADA:
        analisarChamada(n, 0);
        break;

    case NO_IF:
        verificarCondicao(n->filho[0], "if");
        analisarComando(n->filho[1]);
        analisarComando(n->filho[2]);
        break;

    case NO_WHILE:
        verificarCondicao(n->filho[0], "while");
        profundidadeLaco++;
        analisarComando(n->filho[1]);
        profundidadeLaco--;
        break;

    case NO_REPEAT:
        profundidadeLaco++;
        analisarComandos(n->filho[0]);
        profundidadeLaco--;
        verificarCondicao(n->filho[1], "until");
        break;

    case NO_FOR:
        analisarFor(n);
        break;

    case NO_BREAK:
    case NO_CONTINUE:
        if (profundidadeLaco == 0)
            erroToken(n->tk, "'%s' so pode ser usado dentro de while, repeat ou for", lex(n->tk));
        break;

    default:
        break;
    }
}

static void analisarComandos(No *lista) {
    for (; lista != NULL; lista = lista->prox) analisarComando(lista);
}

/* ---------- declaracoes ---------- */

static void analisarConstante(No *n) {
    TipoDado valor = analisarExpressao(n->filho[0]);
    TipoDado t = n->tipoDeclarado;

    if (n->filho[0] != NULL && !ehConstante(n->filho[0]))
        erroNo(n->filho[0], "O valor da constante '%s' precisa ser fixo (numeros, literais e outras constantes)",
               lex(n->tk));

    if (t.base == T_NENHUM) {
        t = valor;   /* sem tipo escrito: a constante fica com o tipo do valor */
        if (t.base == T_LOGICO) {
            erroNo(n->filho[0], "A constante '%s' nao pode ter valor logico", lex(n->tk));
            t = tipoErro();
        }
    } else {
        verificarTipoDeclarado(n);
        t = n->tipoDeclarado;
        if (t.vetor) {
            erroToken(n->tk, "A constante '%s' nao pode ser vetor", lex(n->tk));
            t = tipoErro();
        } else if (!atribuivel(t, valor)) {
            erroNo(n->filho[0], "A constante '%s' e %s, mas o valor e %s", lex(n->tk), nomeTipo(t), nomeTipo(valor));
        }
    }

    n->tipoDado = t;
    n->declaracao = declarar(n->tk, DEC_CONSTANTE, t);
}

static void analisarSubRotina(No *n) {
    int ehFuncao = n->tipo == NO_FUNCAO;
    TipoDado retorno = ehFuncao ? n->tipoDeclarado : tipoSimples(T_NENHUM);
    int indice, primeiro, qtd = 0, lacoSalvo;
    No *p;

    if (ehFuncao && retorno.vetor) {
        erroToken(n->tk, "A funcao '%s' nao pode devolver vetor", lex(n->tk));
        retorno = tipoErro();
    }

    /* o nome entra no escopo de fora, antes do corpo, para permitir recursao */
    indice = declarar(n->tk, ehFuncao ? DEC_FUNCAO : DEC_PROCEDIMENTO, retorno);
    n->declaracao = indice;

    abrirEscopo(nomeId(n->tk), indice);

    /* parametros ficam juntos em declaracoes[], logo depois da sub-rotina */
    primeiro = qtdDeclaracoes;
    for (p = n->filho[0]; p != NULL; p = p->prox) {
        verificarTipoDeclarado(p);
        p->declaracao = declarar(p->tk, DEC_PARAMETRO, p->tipoDeclarado);
        if (p->declaracao >= 0) qtd++;
    }
    if (indice >= 0) {
        declaracoes[indice].primeiroParam = primeiro;
        declaracoes[indice].qtdParams = qtd;
    }

    analisarDeclaracoes(n->filho[1]);

    lacoSalvo = profundidadeLaco;
    profundidadeLaco = 0;
    analisarComando(n->filho[2]);
    profundidadeLaco = lacoSalvo;

    if (ehFuncao && indice >= 0 && !retornoAtribuido[nivel])
        erroToken(n->tk, "A funcao '%s' nunca recebe valor de retorno (falta '%s := ...')",
                  lex(n->tk), lex(n->tk));

    fecharEscopo();
}

static void analisarDeclaracoes(No *lista) {
    No *n;

    for (n = lista; n != NULL; n = n->prox) {
        switch (n->tipo) {
        case NO_VAR:
            verificarTipoDeclarado(n);
            n->declaracao = declarar(n->tk, DEC_VARIAVEL, n->tipoDeclarado);
            break;
        case NO_CONST:
            analisarConstante(n);
            break;
        case NO_PROCEDIMENTO:
        case NO_FUNCAO:
            analisarSubRotina(n);
            break;
        default:
            break;
        }
    }
}

/* ---------- entrada ---------- */

void limparSemantica(void) {
    qtdDeclaracoes = 0;
    qtdVisiveis = 0;
}

void analisarSemantica(void) {
    limparSemantica();
    nivel = 0;
    inicioEscopo[0] = 0;
    subRotinaDoNivel[0] = -1;
    retornoAtribuido[0] = 0;
    strcpy(nomeEscopo[0], "global");
    profundidadeLaco = 0;

    if (raiz == NULL) return;

    predefinir("WRITE");
    predefinir("WRITELN");
    predefinir("READ");
    predefinir("READLN");

    raiz->declaracao = declarar(raiz->tk, DEC_PROGRAMA, tipoSimples(T_NENHUM));
    if (raiz->declaracao >= 0) declaracoes[raiz->declaracao].usada = 1;

    analisarDeclaracoes(raiz->filho[0]);
    analisarComando(raiz->filho[1]);
}

const char *nomeCategoriaDec(CategoriaDec c) {
    switch (c) {
        case DEC_PROGRAMA:     return "programa";
        case DEC_VARIAVEL:     return "variavel";
        case DEC_CONSTANTE:    return "constante";
        case DEC_PARAMETRO:    return "parametro";
        case DEC_PROCEDIMENTO: return "procedimento";
        case DEC_FUNCAO:       return "funcao";
    }
    return "?";
}

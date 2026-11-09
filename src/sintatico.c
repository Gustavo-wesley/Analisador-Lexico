/* Analisador sintatico do Pasquick: descendente recursivo (preditivo, LL(1)).

   Cada regra da gramatica abaixo virou uma funcao com o mesmo nome. Para
   decidir qual alternativa seguir basta olhar o token atual (lookahead).

   Gramatica (EBNF: { } = repete 0 ou mais vezes, [ ] = opcional, | = ou)
   ----------------------------------------------------------------------
   programa      ::= PROGRAM ID ; declaracoes bloco .

   declaracoes   ::= { secao_var | secao_const | procedimento | funcao }
   secao_var     ::= VAR item_var { item_var }
   item_var      ::= lista_ids : tipo ;
   secao_const   ::= CONST item_const { item_const }
   item_const    ::= ID [ : tipo ] = expressao ;
   lista_ids     ::= ID { , ID }
   tipo          ::= ( INTEGER | REAL | CHAR | STRING ) [ '[' NUM ']' ]

   procedimento  ::= PROCEDURE ID [ parametros ] ; declaracoes bloco ;
   funcao        ::= FUNCTION ID [ parametros ] : tipo ; declaracoes bloco ;
   parametros    ::= ( [ grupo { ; grupo } [ ; ] ] )
   grupo         ::= lista_ids : tipo

   bloco         ::= BEGIN lista_comandos END
   lista_comandos::= comando { ; comando }
   comando       ::= atrib_ou_chamada | bloco | cmd_if | cmd_while
                   | cmd_repeat | cmd_for | BREAK | CONTINUE | (vazio)
   atrib_ou_chamada ::= ID [ '[' expressao ']' ] := expressao
                      | ID [ argumentos ]
   cmd_if        ::= IF expressao THEN comando [ ELSE comando ]
   cmd_while     ::= WHILE expressao DO comando
   cmd_repeat    ::= REPEAT lista_comandos UNTIL expressao
   cmd_for       ::= FOR ID := expressao TO expressao DO comando
   argumentos    ::= ( [ expressao { , expressao } ] )

   expressao     ::= expr_e { OU expr_e }
   expr_e        ::= expr_rel { E expr_rel }
   expr_rel      ::= expr_simples [ ( = | <> | < | <= | > | >= ) expr_simples ]
   expr_simples  ::= termo { ( + | - ) termo }
   termo         ::= fator { ( * | / ) fator }
   fator         ::= NUM | LITERAL | ( expressao ) | ( + | - ) fator
                   | ID [ '[' expressao ']' | argumentos ]
   ----------------------------------------------------------------------

   Recuperacao de erros (modo panico): ao encontrar um token inesperado, o
   erro e registrado e os tokens sao descartados ate um token de
   sincronizacao (';', 'end', 'begin', inicio de comando ou de declaracao).
   Enquanto esta "em panico" nenhum erro novo e registrado; o panico acaba
   quando um token esperado e casado. Isso evita erros em cascata. */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "sintatico.h"
#include "lexer.h"
#include "simbolos.h"
#include "erros.h"

static int p;          /* indice do token atual (lookahead) em tokens[] */
static int emPanico;   /* 1 depois de um erro, ate casar um token esperado */

/* ---------- conjuntos de sincronizacao (terminam em TK_EOF) ---------- */

static const TipoToken SINC_COMANDO[] = {
    TK_PONTO_VIRGULA, TK_END, TK_UNTIL, TK_BEGIN, TK_IF, TK_WHILE, TK_REPEAT,
    TK_FOR, TK_BREAK, TK_CONTINUE, TK_VAR, TK_CONST, TK_PROCEDURE, TK_FUNCTION,
    TK_EOF
};

static const TipoToken SINC_DECLARACAO[] = {
    TK_PONTO_VIRGULA, TK_BEGIN, TK_VAR, TK_CONST, TK_PROCEDURE, TK_FUNCTION, TK_EOF
};

static const TipoToken SINC_PARAMETROS[] = {
    TK_FECHA_PAR, TK_PONTO_VIRGULA, TK_DOIS_PONTOS, TK_BEGIN, TK_VAR, TK_CONST, TK_EOF
};

/* ---------- funcoes auxiliares ---------- */

static TipoToken la(void) {
    return tokens[p].tipo;
}

static void avancar(void) {
    if (tokens[p].tipo != TK_EOF) p++;
}

static int pertence(TipoToken t, const TipoToken *conjunto) {
    int i;
    for (i = 0; ; i++) {
        if (conjunto[i] == t) return 1;
        if (conjunto[i] == TK_EOF) return 0;
    }
}

/* descarta tokens ate achar um do conjunto (o EOF sempre para) */
static void sincronizar(const TipoToken *conjunto) {
    while (!pertence(la(), conjunto)) avancar();
}

/* como um token esperado aparece na mensagem: 'begin', ';', identificador... */
static const char *descreverEsperado(TipoToken t) {
    static char b[40];
    int i, j;

    if (t == TK_ID) return "identificador";
    if (t == TK_NUM) return "numero";
    if (t == TK_LITERAL) return "literal";
    if (t == TK_EOF) return "fim do arquivo";

    /* palavras reservadas, operadores e pontuacao estao pre-carregados na tabela */
    for (i = 0; i < qtdSimbolos; i++) {
        if (tabelaSimbolos[i].tipo == t && tabelaSimbolos[i].categoria != CAT_IDENTIFICADOR) {
            b[0] = '\'';
            for (j = 0; tabelaSimbolos[i].lexema[j] && j < 30; j++)
                b[j + 1] = (char)tolower((unsigned char)tabelaSimbolos[i].lexema[j]);
            b[j + 1] = '\'';
            b[j + 2] = '\0';
            return b;
        }
    }
    return nomeToken(t);
}

/* como o token atual aparece na mensagem: 'x', 'begin', fim do arquivo */
static const char *descreverAtual(void) {
    static char b[40];
    Token *t = &tokens[p];

    if (t->tipo == TK_EOF) return "fim do arquivo";
    if (strlen(t->lexema) > 24) sprintf(b, "'%.21s...'", t->lexema);
    else sprintf(b, "'%.24s'", t->lexema);
    return b;
}

/* erro apontando o token atual */
static void erroAqui(const char *mensagem) {
    Token *t = &tokens[p];

    if (emPanico) return;
    emPanico = 1;
    adicionarErroFase(FASE_SINTATICA, t->linha, t->coluna,
                      t->tipo == TK_EOF ? 1 : t->tamanho, mensagem);
}

/* erro de "faltou alguma coisa": se o proximo token ja esta em outra linha,
   aponta logo depois do token anterior (ex: o ';' esquecido no fim da linha) */
static void erroFalta(const char *mensagem) {
    Token *anterior;

    if (emPanico) return;
    if (p > 0 && tokens[p - 1].linha < tokens[p].linha) {
        anterior = &tokens[p - 1];
        emPanico = 1;
        adicionarErroFase(FASE_SINTATICA, anterior->linha, anterior->coluna + anterior->tamanho,
                          1, mensagem);
    } else {
        erroAqui(mensagem);
    }
}

/* Consome o token esperado. Retorna o indice dele em tokens[] ou -1 se faltou. */
static int casar(TipoToken esperado) {
    char msg[128];
    int indice;

    if (la() == esperado) {
        indice = p;
        avancar();
        emPanico = 0;
        return indice;
    }

    sprintf(msg, "Esperado %s, encontrado ", descreverEsperado(esperado));
    strcat(msg, descreverAtual());
    erroFalta(msg);
    return -1;
}

/* coloca n (que pode ser uma lista) no fim da lista ini..fim */
static void anexar(No **ini, No **fim, No *n) {
    if (n == NULL || n == *fim) return;
    if (*ini == NULL) *ini = n;
    else (*fim)->prox = n;
    *fim = n;
    while ((*fim)->prox != NULL && (*fim)->prox != n) *fim = (*fim)->prox;
}

static int iniciaDeclaracao(TipoToken t) {
    return t == TK_VAR || t == TK_CONST || t == TK_PROCEDURE || t == TK_FUNCTION;
}

static int iniciaComando(TipoToken t) {
    return t == TK_ID || t == TK_BEGIN || t == TK_IF || t == TK_WHILE || t == TK_REPEAT ||
           t == TK_FOR || t == TK_BREAK || t == TK_CONTINUE;
}

static int ehRelacional(TipoToken t) {
    return t == TK_IGUAL || t == TK_DIFERENTE || t == TK_MENOR || t == TK_MENOR_IGUAL ||
           t == TK_MAIOR || t == TK_MAIOR_IGUAL;
}

/* prototipos (as regras sao mutuamente recursivas) */
static No *declaracoes(void);
static No *bloco(void);
static No *comando(void);
static No *expressao(void);

/* ---------- declaracoes ---------- */

/* ';' no fim de uma declaracao; se faltar, pula ate um ponto seguro */
static void fimDeclaracao(void) {
    if (casar(TK_PONTO_VIRGULA) >= 0) return;
    sincronizar(SINC_DECLARACAO);
    if (la() == TK_PONTO_VIRGULA) casar(TK_PONTO_VIRGULA);
}

/* lista_ids ::= ID { , ID }   (cria um no do tipo pedido para cada nome) */
static No *listaIds(TipoNo tipoNo) {
    No *ini = NULL, *fim = NULL;
    int t = casar(TK_ID);

    if (t >= 0) anexar(&ini, &fim, novoNo(tipoNo, t));
    while (la() == TK_VIRGULA) {
        casar(TK_VIRGULA);
        t = casar(TK_ID);
        if (t >= 0) anexar(&ini, &fim, novoNo(tipoNo, t));
    }
    return ini;
}

/* tipo ::= ( INTEGER | REAL | CHAR | STRING ) [ '[' NUM ']' ] */
static TipoDado tipo(void) {
    TipoDado t = tipoSimples(T_ERRO);
    char msg[128];
    int n;

    switch (la()) {
        case TK_INTEGER: t.base = T_INTEGER; break;
        case TK_REAL:    t.base = T_REAL;    break;
        case TK_CHAR:    t.base = T_CHAR;    break;
        case TK_STRING:  t.base = T_STRING;  break;
        default:
            sprintf(msg, "Esperado um tipo (integer, real, char ou string), encontrado %s",
                    descreverAtual());
            erroFalta(msg);
            return t;
    }
    casar(la());

    if (la() == TK_ABRE_COL) {
        casar(TK_ABRE_COL);
        t.vetor = 1;
        t.tamanho = -1;   /* o semantico reclama se o tamanho for invalido */
        n = casar(TK_NUM);
        if (n >= 0 && !tokens[n].ehReal && tokens[n].valor >= 1 && tokens[n].valor < 1000000)
            t.tamanho = (int)tokens[n].valor;
        casar(TK_FECHA_COL);
    }
    return t;
}

static void definirTipo(No *lista, TipoDado t) {
    for (; lista != NULL; lista = lista->prox) lista->tipoDeclarado = t;
}

/* secao_var ::= VAR item_var { item_var }      item_var ::= lista_ids : tipo ; */
static void secaoVar(No **ini, No **fim) {
    casar(TK_VAR);
    if (la() != TK_ID) {
        char msg[128];
        sprintf(msg, "Esperado o nome de uma variavel, encontrado %s", descreverAtual());
        erroFalta(msg);
        fimDeclaracao();
        return;
    }
    while (la() == TK_ID) {
        No *lista = listaIds(NO_VAR);
        TipoDado t = tipoSimples(T_ERRO);

        if (casar(TK_DOIS_PONTOS) >= 0) t = tipo();
        definirTipo(lista, t);
        anexar(ini, fim, lista);
        fimDeclaracao();
    }
}

/* secao_const ::= CONST item_const { item_const }
   item_const  ::= ID [ : tipo ] = expressao ; */
static void secaoConst(No **ini, No **fim) {
    casar(TK_CONST);
    if (la() != TK_ID) {
        char msg[128];
        sprintf(msg, "Esperado o nome de uma constante, encontrado %s", descreverAtual());
        erroFalta(msg);
        fimDeclaracao();
        return;
    }
    while (la() == TK_ID) {
        No *c = novoNo(NO_CONST, casar(TK_ID));

        c->tipoDeclarado = tipoSimples(T_NENHUM);
        if (la() == TK_DOIS_PONTOS) {
            casar(TK_DOIS_PONTOS);
            c->tipoDeclarado = tipo();
        }
        casar(TK_IGUAL);
        c->filho[0] = expressao();
        anexar(ini, fim, c);
        fimDeclaracao();
    }
}

/* parametros ::= ( [ grupo { ; grupo } [ ; ] ] )      grupo ::= lista_ids : tipo */
static No *parametros(void) {
    No *ini = NULL, *fim = NULL;

    casar(TK_ABRE_PAR);
    while (la() == TK_ID) {
        No *lista = listaIds(NO_PARAMETRO);
        TipoDado t = tipoSimples(T_ERRO);

        if (casar(TK_DOIS_PONTOS) >= 0) t = tipo();
        definirTipo(lista, t);
        anexar(&ini, &fim, lista);

        if (la() == TK_PONTO_VIRGULA) casar(TK_PONTO_VIRGULA);
        else break;
    }
    if (casar(TK_FECHA_PAR) < 0) {
        sincronizar(SINC_PARAMETROS);
        if (la() == TK_FECHA_PAR) casar(TK_FECHA_PAR);
    }
    return ini;
}

/* procedimento ::= PROCEDURE ID [ parametros ] ; declaracoes bloco ;
   funcao       ::= FUNCTION ID [ parametros ] : tipo ; declaracoes bloco ; */
static No *subRotina(int ehFuncao) {
    int palavra = casar(ehFuncao ? TK_FUNCTION : TK_PROCEDURE);
    int nome = casar(TK_ID);
    No *s = novoNo(ehFuncao ? NO_FUNCAO : NO_PROCEDIMENTO, nome >= 0 ? nome : palavra);

    if (la() == TK_ABRE_PAR) s->filho[0] = parametros();
    if (ehFuncao) {
        s->tipoDeclarado = tipoSimples(T_ERRO);
        if (casar(TK_DOIS_PONTOS) >= 0) s->tipoDeclarado = tipo();
    }
    if (casar(TK_PONTO_VIRGULA) < 0) {
        sincronizar(SINC_DECLARACAO);
        if (la() == TK_PONTO_VIRGULA) casar(TK_PONTO_VIRGULA);
    }

    s->filho[1] = declaracoes();
    s->filho[2] = bloco();
    fimDeclaracao();
    return s;
}

/* declaracoes ::= { secao_var | secao_const | procedimento | funcao } */
static No *declaracoes(void) {
    No *ini = NULL, *fim = NULL;

    while (iniciaDeclaracao(la())) {
        switch (la()) {
            case TK_VAR:       secaoVar(&ini, &fim); break;
            case TK_CONST:     secaoConst(&ini, &fim); break;
            case TK_PROCEDURE: anexar(&ini, &fim, subRotina(0)); break;
            case TK_FUNCTION:  anexar(&ini, &fim, subRotina(1)); break;
            default: break;
        }
    }
    return ini;
}

/* ---------- comandos ---------- */

/* lista_comandos ::= comando { ; comando } */
static No *listaComandos(void) {
    No *ini = NULL, *fim = NULL;
    char msg[128];

    anexar(&ini, &fim, comando());
    while (1) {
        TipoToken t = la();

        if (t == TK_PONTO_VIRGULA) {
            casar(TK_PONTO_VIRGULA);
            anexar(&ini, &fim, comando());
        }
        else if (t == TK_END || t == TK_UNTIL || t == TK_EOF || iniciaDeclaracao(t)) {
            break;   /* quem chamou confere se veio o 'end' / 'until' */
        }
        else if (iniciaComando(t)) {
            /* faltou o ';' entre dois comandos: avisa e segue como se ele estivesse la */
            sprintf(msg, "Esperado ';' antes de %s", descreverAtual());
            erroFalta(msg);
            anexar(&ini, &fim, comando());
        }
        else if (t == TK_ELSE) {
            erroAqui("'else' inesperado: nao coloque ';' antes do else");
            avancar();
            anexar(&ini, &fim, comando());
        }
        else {
            sprintf(msg, "Esperado ';' ou 'end', encontrado %s", descreverAtual());
            erroAqui(msg);
            sincronizar(SINC_COMANDO);
        }
    }
    return ini;
}

/* bloco ::= BEGIN lista_comandos END */
static No *bloco(void) {
    int b = casar(TK_BEGIN);
    No *n = novoNo(NO_BLOCO, b >= 0 ? b : p);

    n->filho[0] = listaComandos();
    casar(TK_END);
    return n;
}

/* argumentos ::= ( [ expressao { , expressao } ] ) */
static No *argumentos(void) {
    No *ini = NULL, *fim = NULL;

    casar(TK_ABRE_PAR);
    if (la() != TK_FECHA_PAR) {
        anexar(&ini, &fim, expressao());
        while (la() == TK_VIRGULA) {
            casar(TK_VIRGULA);
            anexar(&ini, &fim, expressao());
        }
    }
    casar(TK_FECHA_PAR);
    return ini;
}

/* atrib_ou_chamada ::= ID [ '[' expressao ']' ] := expressao  |  ID [ argumentos ] */
static No *atribuicaoOuChamada(void) {
    int id = casar(TK_ID);
    No *n;

    if (la() == TK_ABRE_COL || la() == TK_ATRIB || la() == TK_IGUAL) {
        No *destino;
        int op;

        if (la() == TK_ABRE_COL) {
            destino = novoNo(NO_INDICE, id);
            casar(TK_ABRE_COL);
            destino->filho[0] = expressao();
            casar(TK_FECHA_COL);
        } else {
            destino = novoNo(NO_ID, id);
        }

        if (la() == TK_IGUAL) {
            /* erro comum: x = 1 em vez de x := 1. Avisa e continua como atribuicao */
            erroAqui("Use ':=' para atribuicao ('=' e comparacao)");
            op = p;
            avancar();
        } else {
            op = casar(TK_ATRIB);
        }

        n = novoNo(NO_ATRIB, op >= 0 ? op : id);
        n->filho[0] = destino;
        n->filho[1] = expressao();
        return n;
    }

    n = novoNo(NO_CHAMADA, id);
    if (la() == TK_ABRE_PAR) n->filho[0] = argumentos();
    return n;
}

/* cmd_if ::= IF expressao THEN comando [ ELSE comando ] */
static No *comandoIf(void) {
    No *n = novoNo(NO_IF, casar(TK_IF));

    n->filho[0] = expressao();
    casar(TK_THEN);
    n->filho[1] = comando();
    if (la() == TK_ELSE) {
        casar(TK_ELSE);
        n->filho[2] = comando();
    }
    return n;
}

/* cmd_while ::= WHILE expressao DO comando */
static No *comandoWhile(void) {
    No *n = novoNo(NO_WHILE, casar(TK_WHILE));

    n->filho[0] = expressao();
    casar(TK_DO);
    n->filho[1] = comando();
    return n;
}

/* cmd_repeat ::= REPEAT lista_comandos UNTIL expressao */
static No *comandoRepeat(void) {
    No *n = novoNo(NO_REPEAT, casar(TK_REPEAT));

    n->filho[0] = listaComandos();
    casar(TK_UNTIL);
    n->filho[1] = expressao();
    return n;
}

/* cmd_for ::= FOR ID := expressao TO expressao DO comando */
static No *comandoFor(void) {
    No *n = novoNo(NO_FOR, casar(TK_FOR));
    int v = casar(TK_ID);

    if (v >= 0) n->filho[0] = novoNo(NO_ID, v);
    casar(TK_ATRIB);
    n->filho[1] = expressao();
    casar(TK_TO);
    n->filho[2] = expressao();
    casar(TK_DO);
    n->filho[3] = comando();
    return n;
}

static No *comando(void) {
    char msg[128];

    switch (la()) {
        case TK_ID:       return atribuicaoOuChamada();
        case TK_BEGIN:    return bloco();
        case TK_IF:       return comandoIf();
        case TK_WHILE:    return comandoWhile();
        case TK_REPEAT:   return comandoRepeat();
        case TK_FOR:      return comandoFor();
        case TK_BREAK:    return novoNo(NO_BREAK, casar(TK_BREAK));
        case TK_CONTINUE: return novoNo(NO_CONTINUE, casar(TK_CONTINUE));

        /* comando vazio: permite "begin end" e o ';' antes do 'end' */
        case TK_PONTO_VIRGULA:
        case TK_END:
        case TK_UNTIL:
        case TK_ELSE:
        case TK_EOF:      /* arquivo acabou: quem chamou avisa que faltou o 'end' */
            return NULL;

        default:
            sprintf(msg, "Esperado um comando, encontrado %s", descreverAtual());
            erroAqui(msg);
            return NULL;
    }
}

/* ---------- expressoes (da menor para a maior precedencia) ---------- */

static No *binario(int op, No *esquerda, No *direita) {
    No *n = novoNo(NO_BINARIO, op);
    n->op = tokens[op].tipo;
    n->filho[0] = esquerda;
    n->filho[1] = direita;
    return n;
}

static No *fator(void) {
    char msg[128];
    No *n;
    int t;

    switch (la()) {
    case TK_NUM:
        return novoNo(NO_NUM, casar(TK_NUM));

    case TK_LITERAL:
        return novoNo(NO_LITERAL, casar(TK_LITERAL));

    case TK_ID:
        t = casar(TK_ID);
        if (la() == TK_ABRE_COL) {
            n = novoNo(NO_INDICE, t);
            casar(TK_ABRE_COL);
            n->filho[0] = expressao();
            casar(TK_FECHA_COL);
        } else if (la() == TK_ABRE_PAR) {
            n = novoNo(NO_CHAMADA, t);
            n->filho[0] = argumentos();
        } else {
            n = novoNo(NO_ID, t);
        }
        return n;

    case TK_ABRE_PAR:
        casar(TK_ABRE_PAR);
        n = expressao();
        casar(TK_FECHA_PAR);
        return n;

    case TK_MAIS:
    case TK_MENOS:
        t = casar(la());
        n = novoNo(NO_UNARIO, t);
        n->op = tokens[t].tipo;
        n->filho[0] = fator();
        return n;

    default:
        sprintf(msg, "Esperado uma expressao, encontrado %s", descreverAtual());
        erroFalta(msg);
        return NULL;
    }
}

/* termo ::= fator { ( * | / ) fator } */
static No *termo(void) {
    No *n = fator();
    while (la() == TK_MULT || la() == TK_DIV) {
        int op = casar(la());
        n = binario(op, n, fator());
    }
    return n;
}

/* expr_simples ::= termo { ( + | - ) termo } */
static No *expressaoSimples(void) {
    No *n = termo();
    while (la() == TK_MAIS || la() == TK_MENOS) {
        int op = casar(la());
        n = binario(op, n, termo());
    }
    return n;
}

/* expr_rel ::= expr_simples [ relacional expr_simples ] */
static No *expressaoRelacional(void) {
    No *n = expressaoSimples();
    if (ehRelacional(la())) {
        int op = casar(la());
        n = binario(op, n, expressaoSimples());
    }
    return n;
}

/* expr_e ::= expr_rel { E expr_rel } */
static No *expressaoE(void) {
    No *n = expressaoRelacional();
    while (la() == TK_E) {
        int op = casar(TK_E);
        n = binario(op, n, expressaoRelacional());
    }
    return n;
}

/* expressao ::= expr_e { OU expr_e } */
static No *expressao(void) {
    No *n = expressaoE();
    while (la() == TK_OU) {
        int op = casar(TK_OU);
        n = binario(op, n, expressaoE());
    }
    return n;
}

/* ---------- programa ---------- */

/* programa ::= PROGRAM ID ; declaracoes bloco . */
static No *programa(void) {
    No *prog, *ini = NULL, *fim = NULL;
    char msg[128];
    int nome;

    casar(TK_PROGRAM);
    nome = casar(TK_ID);
    prog = novoNo(NO_PROGRAMA, nome >= 0 ? nome : p);
    if (casar(TK_PONTO_VIRGULA) < 0) {
        sincronizar(SINC_DECLARACAO);
        if (la() == TK_PONTO_VIRGULA) casar(TK_PONTO_VIRGULA);
    }

    /* declaracoes; se aparecer algo estranho antes do begin, pula e tenta de novo */
    while (1) {
        anexar(&ini, &fim, declaracoes());
        if (la() == TK_BEGIN || la() == TK_EOF) break;

        sprintf(msg, "Esperado 'begin' ou uma declaracao (var, const, procedure, function), "
                     "encontrado %s", descreverAtual());
        erroAqui(msg);
        sincronizar(SINC_DECLARACAO);
        if (la() == TK_PONTO_VIRGULA) casar(TK_PONTO_VIRGULA);
    }
    prog->filho[0] = ini;
    prog->filho[1] = bloco();
    casar(TK_PONTO);

    if (la() != TK_EOF) {
        emPanico = 0;
        erroAqui("Texto depois do 'end.' que fecha o programa");
    }
    return prog;
}

void analisarSintaxe(void) {
    limparArvore();
    p = 0;
    emPanico = 0;

    /* o lexer sempre termina com EOF, a nao ser que o vetor de tokens tenha enchido */
    if (qtdTokens == 0) return;
    if (tokens[qtdTokens - 1].tipo != TK_EOF) {
        tokens[qtdTokens - 1].tipo = TK_EOF;
        adicionarErroFase(FASE_SINTATICA, tokens[qtdTokens - 1].linha, tokens[qtdTokens - 1].coluna,
                          1, "Programa grande demais: o resto do arquivo foi ignorado");
    }

    raiz = programa();
}

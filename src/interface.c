#include <stdio.h>
#include <string.h>
#include "interface.h"
#include "lexer.h"
#include "simbolos.h"
#include "erros.h"
#include "arvore.h"
#include "semantico.h"
#include "compilador.h"

Font fonteMono;
float tamFonte = 18;
float charW = 10;
float charH = 22;
int abaAtual = ABA_TOKENS;

static int scrollPainel[QTD_ABAS];

const char *nomeFaseTela(Fase fase) {
    switch (fase) {
        case FASE_LEXICA:    return "léxico";
        case FASE_SINTATICA: return "sintático";
        case FASE_SEMANTICA: return "semântico";
    }
    return "?";
}

void carregarFonte(void) {
    int codepoints[224];
    int i;
    const char *caminho = "C:/Windows/Fonts/consola.ttf";

    /* carrega ASCII + Latin-1 (32 a 255) para os acentos aparecerem */
    for (i = 0; i < 224; i++) codepoints[i] = 32 + i;

    if (FileExists(caminho)) {
        fonteMono = LoadFontEx(caminho, (int)tamFonte, codepoints, 224);
    } else {
        fonteMono = GetFontDefault();
    }
    SetTextureFilter(fonteMono.texture, TEXTURE_FILTER_BILINEAR);

    charW = MeasureTextEx(fonteMono, "M", tamFonte, 0).x;
    charH = tamFonte + 5;
}

void escrever(const char *texto, float x, float y, Color cor) {
    DrawTextEx(fonteMono, texto, (Vector2){ x, y }, tamFonte, 0, cor);
}

int botao(Rectangle r, const char *rotulo) {
    Vector2 mouse = GetMousePosition();
    int emCima = CheckCollisionPointRec(mouse, r);
    float largura = MeasureTextEx(fonteMono, rotulo, tamFonte, 0).x;

    DrawRectangleRec(r, emCima ? COR_DESTAQUE : COR_FUNDO);
    DrawRectangleLinesEx(r, 1, emCima ? COR_RESERVADA : COR_APAGADO);
    escrever(rotulo, r.x + (r.width - largura) / 2, r.y + (r.height - tamFonte) / 2, COR_TEXTO);

    return emCima && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/* corta o texto para caber na coluna da tabela (sem quebrar acento no meio) */
static const char *cortar(const char *s, int max) {
    static char buffer[64];
    int n = (int)strlen(s);

    if (n <= max) return s;
    if (max > 60) max = 60;
    n = max - 3;
    while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
    memcpy(buffer, s, n);
    strcpy(buffer + n, "...");
    return buffer;
}

static void desenharLinhaToken(int i, float x, float y) {
    Token *t = &tokens[i];
    const char *atributo = "-";

    if (t->tipo == TK_NUM)
        atributo = TextFormat("%s, valor = %g", t->ehReal ? "real" : "inteiro", t->valor);
    else if (t->indiceTS >= 0)
        atributo = TextFormat("TS[%d]", t->indiceTS);

    escrever(TextFormat("%d:%d", t->linha, t->coluna), x, y, COR_APAGADO);
    escrever(nomeToken(t->tipo), x + 10 * charW, y, COR_RESERVADA);
    escrever(cortar(t->lexema, 22), x + 26 * charW, y, COR_TEXTO);
    escrever(atributo, x + 50 * charW, y, COR_NUMERO);
}

static void desenharLinhaSimbolo(int i, float x, float y) {
    Simbolo *s = &tabelaSimbolos[i];
    Color cor = s->categoria == CAT_IDENTIFICADOR ? COR_MARCA : COR_TEXTO;

    escrever(TextFormat("%d", i), x, y, COR_APAGADO);
    escrever(s->lexema, x + 6 * charW, y, cor);
    escrever(nomeToken(s->tipo), x + 24 * charW, y, COR_RESERVADA);
    escrever(nomeCategoria(s->categoria), x + 40 * charW, y, COR_TEXTO);
    escrever(s->linha > 0 ? TextFormat("%d", s->linha) : "-", x + 60 * charW, y, COR_APAGADO);
}

/* arvore: o tipo calculado pelo semantico ("   <integer>") sai em outra cor */
static void desenharLinhaArvore(int i, float x, float y) {
    const char *t = linhasArvore[i].texto;
    const char *tipo = strstr(t, "   <");
    char parte[100];
    int n;

    if (tipo == NULL) {
        escrever(t, x, y, COR_TEXTO);
        return;
    }
    n = (int)(tipo - t);
    memcpy(parte, t, n);
    parte[n] = '\0';
    escrever(parte, x, y, COR_TEXTO);
    escrever(tipo, x + n * charW, y, COR_NUMERO);
}

static void desenharLinhaDeclaracao(int i, float x, float y) {
    Declaracao *d = &declaracoes[i];
    const char *tipo = (d->categoria == DEC_PROCEDIMENTO || d->categoria == DEC_PROGRAMA)
                       ? "-" : nomeTipo(d->tipo);

    escrever(TextFormat("%d", i), x, y, COR_APAGADO);
    escrever(d->nome, x + 5 * charW, y, d->predefinida ? COR_APAGADO : COR_MARCA);
    escrever(nomeCategoriaDec(d->categoria), x + 22 * charW, y, COR_RESERVADA);
    escrever(tipo, x + 36 * charW, y, COR_NUMERO);
    escrever(d->escopo, x + 50 * charW, y, COR_TEXTO);
    if (d->predefinida) {
        escrever("pré-definido", x + 66 * charW, y, COR_APAGADO);
    } else {
        escrever(TextFormat("%d", d->linha), x + 66 * charW, y, COR_APAGADO);
        escrever(d->usada ? "sim" : "não", x + 74 * charW, y, d->usada ? COR_TEXTO : COR_MARCA);
    }
}

static void desenharLinhaErro(int i, float x, float y) {
    Erro *e = &erros[i];
    escrever(TextFormat("Linha %d, coluna %d", e->linha, e->coluna), x, y, COR_ERRO);
    escrever(nomeFaseTela(e->fase), x + 22 * charW, y, COR_MARCA);
    escrever(e->mensagem, x + 34 * charW, y, COR_TEXTO);
}

int desenharPainel(Rectangle area, int *linhaDestino, int *colunaDestino) {
    Vector2 mouse = GetMousePosition();
    float alturaAbas = 32;
    Rectangle conteudo = { area.x, area.y + alturaAbas, area.width, area.height - alturaAbas };
    int linhasVisiveis = (int)((conteudo.height - charH - 8) / charH);
    int total, i, clicou = 0;
    float x = conteudo.x + 12;
    float y;

    DrawRectangleRec(area, COR_PAINEL);
    DrawLine((int)area.x, (int)area.y, (int)(area.x + area.width), (int)area.y, COR_BARRA);

    /* abas */
    for (i = 0; i < QTD_ABAS; i++) {
        Rectangle r = { area.x + 6 + i * 176, area.y + 4, 170, alturaAbas - 4 };
        const char *rotulo;
        Color cor = COR_TEXTO;
        float largura;

        if (i == ABA_TOKENS) rotulo = TextFormat("Tokens (%d)", qtdTokens);
        else if (i == ABA_SIMBOLOS) rotulo = TextFormat("Símbolos (%d)", qtdSimbolos);
        else if (i == ABA_ARVORE) rotulo = TextFormat("Árvore (%d)", qtdLinhasArvore);
        else if (i == ABA_DECLARACOES) rotulo = TextFormat("Declarações (%d)", qtdDeclaracoes);
        else {
            rotulo = TextFormat("Erros (%d)", qtdErros);
            cor = qtdErros > 0 ? COR_ERRO : COR_OK;
        }

        DrawRectangleRec(r, i == abaAtual ? COR_FUNDO : COR_BARRA);
        if (i == abaAtual) DrawRectangle((int)r.x, (int)r.y, (int)r.width, 2, COR_RESERVADA);
        largura = MeasureTextEx(fonteMono, rotulo, tamFonte, 0).x;
        escrever(rotulo, r.x + (r.width - largura) / 2, r.y + (r.height - tamFonte) / 2, cor);

        if (CheckCollisionPointRec(mouse, r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) abaAtual = i;
    }

    DrawRectangleRec(conteudo, COR_FUNDO);

    if (abaAtual == ABA_TOKENS) total = qtdTokens;
    else if (abaAtual == ABA_SIMBOLOS) total = qtdSimbolos;
    else if (abaAtual == ABA_ARVORE) total = qtdLinhasArvore;
    else if (abaAtual == ABA_DECLARACOES) total = semanticaExecutada ? qtdDeclaracoes : 0;
    else total = qtdErros;

    /* rolagem com a rodinha do mouse */
    if (CheckCollisionPointRec(mouse, conteudo)) {
        scrollPainel[abaAtual] -= (int)(GetMouseWheelMove() * 3);
    }
    if (scrollPainel[abaAtual] > total - linhasVisiveis) scrollPainel[abaAtual] = total - linhasVisiveis;
    if (scrollPainel[abaAtual] < 0) scrollPainel[abaAtual] = 0;

    /* cabecalho */
    y = conteudo.y + 6;
    if (abaAtual == ABA_TOKENS) {
        escrever("LIN:COL", x, y, COR_APAGADO);
        escrever("TOKEN", x + 10 * charW, y, COR_APAGADO);
        escrever("LEXEMA", x + 26 * charW, y, COR_APAGADO);
        escrever("ATRIBUTO", x + 50 * charW, y, COR_APAGADO);
    } else if (abaAtual == ABA_SIMBOLOS) {
        escrever("IND", x, y, COR_APAGADO);
        escrever("LEXEMA", x + 6 * charW, y, COR_APAGADO);
        escrever("TOKEN", x + 24 * charW, y, COR_APAGADO);
        escrever("CATEGORIA", x + 40 * charW, y, COR_APAGADO);
        escrever("LINHA", x + 60 * charW, y, COR_APAGADO);
    } else if (abaAtual == ABA_ARVORE) {
        if (!faseLigada(FASE_SINTATICA))
            escrever("Análise sintática desligada: mude a fase com F6.", x, y, COR_MARCA);
        else if (faseLigada(FASE_SEMANTICA))
            escrever("ÁRVORE SINTÁTICA  (entre < > o tipo calculado pelo semântico; clique para ir ao código)",
                     x, y, COR_APAGADO);
        else
            escrever("ÁRVORE SINTÁTICA  (sem tipos: o semântico está desligado; clique para ir ao código)",
                     x, y, COR_APAGADO);
    } else if (abaAtual == ABA_DECLARACOES) {
        if (!faseLigada(FASE_SEMANTICA)) {
            escrever("Análise semântica desligada: mude a fase com F6.", x, y, COR_MARCA);
        } else if (!semanticaExecutada) {
            escrever("Análise semântica não executada: corrija primeiro os erros sintáticos.", x, y, COR_MARCA);
        } else {
            escrever("IND", x, y, COR_APAGADO);
            escrever("NOME", x + 5 * charW, y, COR_APAGADO);
            escrever("CATEGORIA", x + 22 * charW, y, COR_APAGADO);
            escrever("TIPO", x + 36 * charW, y, COR_APAGADO);
            escrever("ESCOPO", x + 50 * charW, y, COR_APAGADO);
            escrever("LINHA", x + 66 * charW, y, COR_APAGADO);
            escrever("USADA", x + 74 * charW, y, COR_APAGADO);
        }
    } else if (qtdErros == 0) {
        if (faseFinal == FASE_SEMANTICA)
            escrever("Nenhum erro encontrado: o programa está correto.", x, y, COR_OK);
        else
            escrever(TextFormat("Nenhum erro até a fase %s.", nomeFaseTela(faseFinal)), x, y, COR_OK);
    } else {
        escrever("POSIÇÃO", x, y, COR_APAGADO);
        escrever("FASE", x + 22 * charW, y, COR_APAGADO);
        escrever("MENSAGEM  (clique para ir até o erro)", x + 34 * charW, y, COR_APAGADO);
    }
    y += charH;

    /* linhas da tabela */
    BeginScissorMode((int)conteudo.x, (int)y, (int)conteudo.width, (int)(conteudo.y + conteudo.height - y));
    for (i = scrollPainel[abaAtual]; i < total && i < scrollPainel[abaAtual] + linhasVisiveis + 1; i++) {
        Rectangle linhaR = { conteudo.x, y - 2, conteudo.width, charH };
        int emCima = CheckCollisionPointRec(mouse, linhaR);

        if (emCima) DrawRectangleRec(linhaR, COR_DESTAQUE);

        if (abaAtual == ABA_TOKENS) desenharLinhaToken(i, x, y);
        else if (abaAtual == ABA_SIMBOLOS) desenharLinhaSimbolo(i, x, y);
        else if (abaAtual == ABA_ARVORE) desenharLinhaArvore(i, x, y);
        else if (abaAtual == ABA_DECLARACOES) desenharLinhaDeclaracao(i, x, y);
        else desenharLinhaErro(i, x, y);

        /* clicar numa linha leva o cursor do editor para aquela posicao */
        if (emCima && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (abaAtual == ABA_TOKENS) {
                *linhaDestino = tokens[i].linha;
                *colunaDestino = tokens[i].coluna;
                clicou = 1;
            } else if (abaAtual == ABA_ERROS) {
                *linhaDestino = erros[i].linha;
                *colunaDestino = erros[i].coluna;
                clicou = 1;
            } else if (abaAtual == ABA_ARVORE && linhasArvore[i].linha > 0) {
                *linhaDestino = linhasArvore[i].linha;
                *colunaDestino = linhasArvore[i].coluna;
                clicou = 1;
            } else if (abaAtual == ABA_DECLARACOES && declaracoes[i].linha > 0) {
                *linhaDestino = declaracoes[i].linha;
                *colunaDestino = declaracoes[i].coluna;
                clicou = 1;
            } else if (abaAtual == ABA_SIMBOLOS && tabelaSimbolos[i].linha > 0) {
                *linhaDestino = tabelaSimbolos[i].linha;
                *colunaDestino = 1;
                clicou = 1;
            }
        }
        y += charH;
    }
    EndScissorMode();

    return clicou;
}

void desenharBarraStatus(Rectangle area, int linhaCursor, int colunaCursor) {
    int i;
    const char *esquerda = TextFormat("Pasquick   Lin %d, Col %d   |   %d tokens   |   %d símbolos"
                                      "   |   fase: até o %s",
                                      linhaCursor, colunaCursor, qtdTokens, qtdSimbolos,
                                      nomeFaseTela(faseFinal));
    float y = area.y + (area.height - tamFonte) / 2;

    DrawRectangleRec(area, qtdErros > 0 ? (Color){ 110, 30, 30, 255 } : (Color){ 0, 90, 156, 255 });
    escrever(esquerda, area.x + 10, y, RAYWHITE);

    /* mostra o erro da linha onde esta o cursor */
    for (i = 0; i < qtdErros; i++) {
        if (erros[i].linha == linhaCursor) {
            const char *msg = TextFormat("Erro %s: %s", nomeFaseTela(erros[i].fase), erros[i].mensagem);
            float largura = MeasureTextEx(fonteMono, msg, tamFonte, 0).x;
            escrever(msg, area.x + area.width - largura - 10, y, RAYWHITE);
            return;
        }
    }
    {
        const char *msg = qtdErros > 0 ? TextFormat("%d erro(s)", qtdErros) : "Programa correto";
        float largura = MeasureTextEx(fonteMono, msg, tamFonte, 0).x;
        escrever(msg, area.x + area.width - largura - 10, y, RAYWHITE);
    }
}

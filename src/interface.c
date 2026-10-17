#include <stdio.h>
#include <string.h>
#include "interface.h"
#include "lexer.h"
#include "simbolos.h"
#include "erros.h"

Font fonteMono;
float tamFonte = 18;
float charW = 10;
float charH = 22;
int abaAtual = ABA_TOKENS;

static int scrollPainel[3] = { 0, 0, 0 };

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

static void desenharLinhaErro(int i, float x, float y) {
    Erro *e = &erros[i];
    escrever(TextFormat("Linha %d, coluna %d", e->linha, e->coluna), x, y, COR_ERRO);
    escrever(e->mensagem, x + 24 * charW, y, COR_TEXTO);
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
    for (i = 0; i < 3; i++) {
        Rectangle r = { area.x + 6 + i * 190, area.y + 4, 184, alturaAbas - 4 };
        const char *rotulo;
        Color cor = COR_TEXTO;
        float largura;

        if (i == ABA_TOKENS) rotulo = TextFormat("Tokens (%d)", qtdTokens);
        else if (i == ABA_SIMBOLOS) rotulo = TextFormat("Símbolos (%d)", qtdSimbolos);
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
    } else if (qtdErros == 0) {
        escrever("Nenhum erro léxico encontrado.", x, y, COR_OK);
    } else {
        escrever("POSIÇÃO", x, y, COR_APAGADO);
        escrever("MENSAGEM  (clique para ir até o erro)", x + 24 * charW, y, COR_APAGADO);
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
            } else if (tabelaSimbolos[i].linha > 0) {
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
    const char *esquerda = TextFormat("Pasquick   Lin %d, Col %d   |   %d tokens   |   %d símbolos",
                                      linhaCursor, colunaCursor, qtdTokens, qtdSimbolos);
    float y = area.y + (area.height - tamFonte) / 2;

    DrawRectangleRec(area, qtdErros > 0 ? (Color){ 110, 30, 30, 255 } : (Color){ 0, 90, 156, 255 });
    escrever(esquerda, area.x + 10, y, RAYWHITE);

    /* mostra o erro da linha onde esta o cursor */
    for (i = 0; i < qtdErros; i++) {
        if (erros[i].linha == linhaCursor) {
            const char *msg = TextFormat("Erro: %s", erros[i].mensagem);
            float largura = MeasureTextEx(fonteMono, msg, tamFonte, 0).x;
            escrever(msg, area.x + area.width - largura - 10, y, RAYWHITE);
            return;
        }
    }
    {
        const char *msg = qtdErros > 0 ? TextFormat("%d erro(s) léxico(s)", qtdErros) : "Sem erros léxicos";
        float largura = MeasureTextEx(fonteMono, msg, tamFonte, 0).x;
        escrever(msg, area.x + area.width - largura - 10, y, RAYWHITE);
    }
}

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "editor.h"
#include "interface.h"
#include "lexer.h"
#include "erros.h"

char texto[MAX_TEXTO];
int tamTexto = 0;
char caminhoArquivo[512] = "";
int modificado = 0;
int textoAlterado = 0;

static int cursor = 0;
static int colunaDesejada = 1;     /* para as setas cima/baixo lembrarem a coluna */
static int inicioLinha[MAX_LINHAS];
static int qtdLinhas = 1;
static int scrollLinha = 0;
static int scrollColuna = 0;
static int linhasVisiveis = 20;
static int colunasVisiveis = 80;
static double ultimoMovimento = 0;

/* classe de cada byte do texto, para colorir (vem dos tokens) */
typedef enum {
    CLASSE_OUTRO,       /* comentario (o lexer descarta, entao nao vira token) */
    CLASSE_RESERVADA,
    CLASSE_ID,
    CLASSE_NUMERO,
    CLASSE_LITERAL,
    CLASSE_OPERADOR,
    CLASSE_ERRO         /* trecho com erro lexico (tambem nao vira token) */
} Classe;
static unsigned char classe[MAX_TEXTO];

/* ---------- funcoes auxiliares ---------- */

static int ehContinuacaoUtf8(char c) {
    return ((unsigned char)c & 0xC0) == 0x80;
}

static void recalcularLinhas(void) {
    int i;
    qtdLinhas = 1;
    inicioLinha[0] = 0;
    for (i = 0; i < tamTexto; i++) {
        if (texto[i] == '\n' && qtdLinhas < MAX_LINHAS) {
            inicioLinha[qtdLinhas++] = i + 1;
        }
    }
}

static void marcarAlterado(void) {
    recalcularLinhas();
    modificado = 1;
    textoAlterado = 1;
}

/* fim da linha (posicao do '\n' ou do fim do texto); linha comeca em 0 */
static int fimLinha(int linha) {
    if (linha + 1 < qtdLinhas) return inicioLinha[linha + 1] - 1;
    return tamTexto;
}

static int linhaDaPosicao(int p) {
    int l = 0;
    while (l + 1 < qtdLinhas && inicioLinha[l + 1] <= p) l++;
    return l;
}

/* coluna (comecando em 1) da posicao p, contando caracteres e nao bytes */
static int colunaDaPosicao(int p) {
    int i, col = 1;
    for (i = inicioLinha[linhaDaPosicao(p)]; i < p; i++) {
        if (!ehContinuacaoUtf8(texto[i])) col++;
    }
    return col;
}

/* posicao (byte) da linha (0..) e coluna (1..); para no fim da linha se ela for curta */
static int posicaoDe(int linha, int coluna) {
    int p, col = 1;

    if (linha < 0) linha = 0;
    if (linha >= qtdLinhas) linha = qtdLinhas - 1;

    p = inicioLinha[linha];
    while (col < coluna && p < fimLinha(linha)) {
        p++;
        while (p < tamTexto && ehContinuacaoUtf8(texto[p])) p++;
        col++;
    }
    return p;
}

static void inserirBytes(const char *s, int n) {
    if (tamTexto + n >= MAX_TEXTO) return;
    memmove(texto + cursor + n, texto + cursor, tamTexto - cursor + 1);
    memcpy(texto + cursor, s, n);
    cursor += n;
    tamTexto += n;
}

/* insere texto vindo de colar (Ctrl+V): tira \r e troca TAB por espacos */
static void inserirTexto(const char *s) {
    for (; *s; s++) {
        if (*s == '\r') continue;
        if (*s == '\t') inserirBytes("    ", 4);
        else inserirBytes(s, 1);
    }
    marcarAlterado();
}

static void apagar(int inicio, int fim) {
    if (inicio < 0 || fim > tamTexto || inicio >= fim) return;
    memmove(texto + inicio, texto + fim, tamTexto - fim + 1);
    tamTexto -= fim - inicio;
    cursor = inicio;
    marcarAlterado();
}

static void garantirCursorVisivel(void) {
    int l = linhaDaPosicao(cursor);
    int c = colunaDaPosicao(cursor) - 1;

    if (l < scrollLinha) scrollLinha = l;
    if (l >= scrollLinha + linhasVisiveis) scrollLinha = l - linhasVisiveis + 1;
    if (c < scrollColuna) scrollColuna = c;
    if (c >= scrollColuna + colunasVisiveis) scrollColuna = c - colunasVisiveis + 1;
}

static int teclaRepetida(int tecla) {
    return IsKeyPressed(tecla) || IsKeyPressedRepeat(tecla);
}

static float larguraGutter(void) {
    return charW * 5 + 12;   /* espaco dos numeros de linha */
}

/* ---------- arquivo ---------- */

void editorNovo(void) {
    strcpy(texto, "program Novo;\nvar\n  x : integer;\nbegin\n  x := 0;\nend.\n");
    tamTexto = (int)strlen(texto);
    caminhoArquivo[0] = '\0';
    cursor = 0;
    scrollLinha = scrollColuna = 0;
    marcarAlterado();
    modificado = 0;
}

int editorCarregar(const char *caminho) {
    FILE *f = fopen(caminho, "rb");
    int c, i = 0;

    if (f == NULL) return 0;

    /* le caractere por caractere: tira \r, troca TAB por 4 espacos e pula o BOM do UTF-8 */
    while ((c = fgetc(f)) != EOF && i < MAX_TEXTO - 5) {
        if (c == '\r') continue;
        if (c == '\t') {
            memcpy(texto + i, "    ", 4);
            i += 4;
        } else {
            texto[i++] = (char)c;
        }
    }
    fclose(f);
    texto[i] = '\0';
    tamTexto = i;

    if (tamTexto >= 3 && (unsigned char)texto[0] == 0xEF &&
        (unsigned char)texto[1] == 0xBB && (unsigned char)texto[2] == 0xBF) {
        memmove(texto, texto + 3, tamTexto - 2);
        tamTexto -= 3;
    }

    strncpy(caminhoArquivo, caminho, sizeof(caminhoArquivo) - 1);
    caminhoArquivo[sizeof(caminhoArquivo) - 1] = '\0';
    cursor = 0;
    scrollLinha = scrollColuna = 0;
    marcarAlterado();
    modificado = 0;
    return 1;
}

int editorSalvar(void) {
    FILE *f;

    if (caminhoArquivo[0] == '\0') strcpy(caminhoArquivo, "programa.pqk");

    f = fopen(caminhoArquivo, "wb");
    if (f == NULL) return 0;
    fwrite(texto, 1, tamTexto, f);
    fclose(f);
    modificado = 0;
    return 1;
}

/* ---------- cursor ---------- */

void editorIrPara(int linha, int coluna) {
    cursor = posicaoDe(linha - 1, coluna);
    colunaDesejada = coluna;
    ultimoMovimento = GetTime();
    garantirCursorVisivel();
}

int editorLinhaCursor(void) {
    return linhaDaPosicao(cursor) + 1;
}

int editorColunaCursor(void) {
    return colunaDaPosicao(cursor);
}

/* ---------- teclado e mouse ---------- */

void editorAtualizar(Rectangle area) {
    int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    int moveu = 0;          /* cursor andou: precisa ficar visivel */
    int horizontal = 0;     /* atualiza a coluna desejada */
    int linha = linhaDaPosicao(cursor);
    int cp;
    Vector2 mouse = GetMousePosition();

    linhasVisiveis = (int)((area.height - 8) / charH);
    colunasVisiveis = (int)((area.width - larguraGutter() - 8) / charW);
    if (linhasVisiveis < 1) linhasVisiveis = 1;
    if (colunasVisiveis < 1) colunasVisiveis = 1;

    /* caracteres digitados (com acento tambem, ate o codigo 255) */
    cp = GetCharPressed();
    while (cp > 0) {
        if (cp >= 32 && cp != 127 && cp <= 255) {
            int n;
            const char *utf8 = CodepointToUTF8(cp, &n);
            inserirBytes(utf8, n);
            marcarAlterado();
            moveu = horizontal = 1;
        }
        cp = GetCharPressed();
    }

    if (teclaRepetida(KEY_ENTER) || teclaRepetida(KEY_KP_ENTER)) {
        /* nova linha mantendo a indentacao da linha atual */
        char quebra[128];
        int n = 0, p = inicioLinha[linha];
        quebra[n++] = '\n';
        while (p < cursor && texto[p] == ' ' && n < 120) { quebra[n++] = ' '; p++; }
        inserirBytes(quebra, n);
        marcarAlterado();
        moveu = horizontal = 1;
    }
    if (teclaRepetida(KEY_TAB)) {
        inserirBytes("  ", 2);
        marcarAlterado();
        moveu = horizontal = 1;
    }
    if (teclaRepetida(KEY_BACKSPACE) && cursor > 0) {
        int ini = cursor - 1;
        while (ini > 0 && ehContinuacaoUtf8(texto[ini])) ini--;
        apagar(ini, cursor);
        moveu = horizontal = 1;
    }
    if (teclaRepetida(KEY_DELETE) && cursor < tamTexto) {
        int fim = cursor + 1;
        while (fim < tamTexto && ehContinuacaoUtf8(texto[fim])) fim++;
        apagar(cursor, fim);
        moveu = horizontal = 1;
    }
    if (ctrl && IsKeyPressed(KEY_V)) {
        const char *colado = GetClipboardText();
        if (colado != NULL) inserirTexto(colado);
        moveu = horizontal = 1;
    }

    /* setas */
    if (teclaRepetida(KEY_LEFT) && cursor > 0) {
        cursor--;
        while (cursor > 0 && ehContinuacaoUtf8(texto[cursor])) cursor--;
        moveu = horizontal = 1;
    }
    if (teclaRepetida(KEY_RIGHT) && cursor < tamTexto) {
        cursor++;
        while (cursor < tamTexto && ehContinuacaoUtf8(texto[cursor])) cursor++;
        moveu = horizontal = 1;
    }
    linha = linhaDaPosicao(cursor);
    if (teclaRepetida(KEY_UP) && linha > 0) {
        cursor = posicaoDe(linha - 1, colunaDesejada);
        moveu = 1;
    }
    if (teclaRepetida(KEY_DOWN) && linha + 1 < qtdLinhas) {
        cursor = posicaoDe(linha + 1, colunaDesejada);
        moveu = 1;
    }
    if (teclaRepetida(KEY_PAGE_UP)) {
        cursor = posicaoDe(linha - linhasVisiveis, colunaDesejada);
        moveu = 1;
    }
    if (teclaRepetida(KEY_PAGE_DOWN)) {
        cursor = posicaoDe(linha + linhasVisiveis, colunaDesejada);
        moveu = 1;
    }
    if (IsKeyPressed(KEY_HOME)) {
        cursor = ctrl ? 0 : inicioLinha[linha];
        moveu = horizontal = 1;
    }
    if (IsKeyPressed(KEY_END)) {
        cursor = ctrl ? tamTexto : fimLinha(linha);
        moveu = horizontal = 1;
    }

    /* mouse: clique posiciona o cursor, rodinha rola o texto */
    if (CheckCollisionPointRec(mouse, area)) {
        float roda = GetMouseWheelMove();
        if (roda != 0) {
            scrollLinha -= (int)(roda * 3);
            if (scrollLinha > qtdLinhas - 1) scrollLinha = qtdLinhas - 1;
            if (scrollLinha < 0) scrollLinha = 0;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            float textoX = area.x + larguraGutter();
            int l = scrollLinha + (int)((mouse.y - area.y - 4) / charH);
            int c = scrollColuna + (int)((mouse.x - textoX) / charW + 0.5f) + 1;
            if (c < 1) c = 1;
            cursor = posicaoDe(l, c);
            horizontal = 1;
            ultimoMovimento = GetTime();
        }
    }

    if (horizontal) colunaDesejada = colunaDaPosicao(cursor);
    if (moveu) {
        ultimoMovimento = GetTime();
        garantirCursorVisivel();
    }
}

/* ---------- cores (syntax highlight) ---------- */

void editorAtualizarCores(void) {
    int i, j;

    memset(classe, CLASSE_OUTRO, tamTexto);

    for (i = 0; i < qtdTokens; i++) {
        Token *t = &tokens[i];
        Classe c;

        if (t->tipo <= TK_TO)           c = CLASSE_RESERVADA;
        else if (t->tipo == TK_ID)      c = CLASSE_ID;
        else if (t->tipo == TK_NUM)     c = CLASSE_NUMERO;
        else if (t->tipo == TK_LITERAL) c = CLASSE_LITERAL;
        else                            c = CLASSE_OPERADOR;

        for (j = t->inicio; j < t->inicio + t->bytes && j < tamTexto; j++) classe[j] = (unsigned char)c;
    }

    /* trechos com erro ficam vermelhos (senao pareceriam comentario) */
    for (i = 0; i < qtdErros; i++) {
        int ini = posicaoDe(erros[i].linha - 1, erros[i].coluna);
        int fim = posicaoDe(erros[i].linha - 1, erros[i].coluna + erros[i].tamanho);
        for (j = ini; j < fim; j++) classe[j] = CLASSE_ERRO;
    }
}

static Color corDaClasse(int c) {
    switch (c) {
        case CLASSE_RESERVADA: return COR_RESERVADA;
        case CLASSE_ID:        return COR_TEXTO;
        case CLASSE_NUMERO:    return COR_NUMERO;
        case CLASSE_LITERAL:   return COR_LITERAL;
        case CLASSE_OPERADOR:  return COR_OPERADOR;
        case CLASSE_ERRO:      return (Color){ 255, 140, 140, 255 };
    }
    return COR_COMENTARIO;
}

/* ---------- desenho ---------- */

/* sublinhado ondulado vermelho, igual ao de corretor ortografico */
static void sublinharErro(float x, float y, float largura) {
    float passo = 3;
    float px = x;
    int sobe = 1;

    while (px < x + largura) {
        float proximo = px + passo;
        if (proximo > x + largura) proximo = x + largura;
        DrawLineEx((Vector2){ px, sobe ? y + 2 : y },
                   (Vector2){ proximo, sobe ? y : y + 2 }, 1.5f, COR_ERRO);
        sobe = !sobe;
        px = proximo;
    }
}

static int linhaTemErro(int linha1) {
    int i;
    for (i = 0; i < qtdErros; i++) {
        if (erros[i].linha == linha1) return 1;
    }
    return 0;
}

void editorDesenhar(Rectangle area) {
    float textoX = area.x + larguraGutter();
    float topo = area.y + 4;
    int linhaCursor = linhaDaPosicao(cursor);
    int l, i;
    Vector2 mouse = GetMousePosition();
    int erroDoMouse = -1;

    DrawRectangleRec(area, COR_FUNDO);
    DrawRectangle((int)area.x, (int)area.y, (int)larguraGutter() - 6, (int)area.height, COR_PAINEL);

    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);

    for (l = scrollLinha; l < qtdLinhas && l < scrollLinha + linhasVisiveis + 1; l++) {
        float y = topo + (l - scrollLinha) * charH;
        int p = inicioLinha[l];
        int fim = fimLinha(l);
        int col = 1;
        char numero[16];

        if (l == linhaCursor) {
            DrawRectangle((int)textoX - 4, (int)y, (int)area.width, (int)charH, COR_DESTAQUE);
        }

        /* numero da linha (vermelho se a linha tem erro) */
        sprintf(numero, "%4d", l + 1);
        escrever(numero, area.x + 4, y + 2,
                 linhaTemErro(l + 1) ? COR_ERRO : (l == linhaCursor ? COR_TEXTO : COR_APAGADO));

        /* desenha caractere por caractere, cada um na sua coluna */
        while (p < fim) {
            int n;
            int cp = GetCodepointNext(&texto[p], &n);
            int colTela = col - 1 - scrollColuna;

            if (colTela >= colunasVisiveis + 1) break;
            if (colTela >= 0 && cp != ' ') {
                DrawTextCodepoint(fonteMono, cp, (Vector2){ textoX + colTela * charW, y + 2 },
                                  tamFonte, corDaClasse(classe[p]));
            }
            p += n;
            col++;
        }
    }

    /* erros: sublinhado vermelho */
    for (i = 0; i < qtdErros; i++) {
        Erro *e = &erros[i];
        int linhaTela = e->linha - 1 - scrollLinha;
        float x, y, largura;

        if (linhaTela < 0 || linhaTela > linhasVisiveis) continue;

        x = textoX + (e->coluna - 1 - scrollColuna) * charW;
        y = topo + linhaTela * charH;
        largura = e->tamanho * charW;
        sublinharErro(x, y + charH - 3, largura);

        if (CheckCollisionPointRec(mouse, (Rectangle){ x, y, largura, charH })) erroDoMouse = i;
    }

    /* cursor piscando */
    if (((int)((GetTime() - ultimoMovimento) * 2)) % 2 == 0) {
        int colTela = colunaDaPosicao(cursor) - 1 - scrollColuna;
        int linhaTela = linhaCursor - scrollLinha;
        if (colTela >= 0 && linhaTela >= 0) {
            float x = textoX + colTela * charW;
            float y = topo + linhaTela * charH;
            DrawRectangle((int)x, (int)y + 1, 2, (int)charH - 2, COR_TEXTO);
        }
    }

    EndScissorMode();

    /* dica com a mensagem quando o mouse passa em cima do erro */
    if (erroDoMouse >= 0) {
        Erro *e = &erros[erroDoMouse];
        const char *msg = TextFormat("Erro léxico (lin %d, col %d): %s", e->linha, e->coluna, e->mensagem);
        float largura = MeasureTextEx(fonteMono, msg, tamFonte, 0).x + 16;
        float x = mouse.x + 12;
        float y = mouse.y + 18;

        if (x + largura > GetScreenWidth()) x = GetScreenWidth() - largura - 4;
        DrawRectangle((int)x, (int)y, (int)largura, (int)charH + 8, COR_BARRA);
        DrawRectangleLines((int)x, (int)y, (int)largura, (int)charH + 8, COR_ERRO);
        escrever(msg, x + 8, y + 6, COR_TEXTO);
    }
}

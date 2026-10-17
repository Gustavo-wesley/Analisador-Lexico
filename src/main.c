/* Chronus - IDE da linguagem Pasquick
   Trabalho 01 de Compiladores: analisador lexico

   Atalhos: F5 compila, Ctrl+S salva, Ctrl+V cola.
   Para abrir um arquivo .pqk, arraste ele para a janela. */

#include <stdio.h>
#include <string.h>
#include "raylib.h"
#include "editor.h"
#include "interface.h"
#include "lexer.h"
#include "erros.h"

static char mensagem[160] = "";
static Color corMensagem;
static double tempoMensagem = 0;
static int confirmarNovo = 0;

static void mostrarMensagem(const char *texto, Color cor) {
    strncpy(mensagem, texto, sizeof(mensagem) - 1);
    mensagem[sizeof(mensagem) - 1] = '\0';
    corMensagem = cor;
    tempoMensagem = GetTime();
}

static void analisar(void) {
    analisarFonte(texto);
    editorAtualizarCores();
    textoAlterado = 0;
}

static void compilar(void) {
    analisar();
    abaAtual = ABA_ERROS;
    if (qtdErros == 0)
        mostrarMensagem("Compilado: nenhum erro léxico", COR_OK);
    else
        mostrarMensagem(TextFormat("Compilado: %d erro(s) léxico(s)", qtdErros), COR_ERRO);
}

static void salvar(void) {
    if (editorSalvar())
        mostrarMensagem(TextFormat("Salvo em %s", GetFileName(caminhoArquivo)), COR_OK);
    else
        mostrarMensagem("Não foi possível salvar o arquivo", COR_ERRO);
}

static void abrir(const char *caminho) {
    if (editorCarregar(caminho)) {
        analisar();
        mostrarMensagem(TextFormat("Aberto: %s", GetFileName(caminho)), COR_OK);
    } else {
        mostrarMensagem("Não foi possível abrir o arquivo", COR_ERRO);
    }
}

static void desenharBarraSuperior(Rectangle area) {
    const char *nome = caminhoArquivo[0] ? GetFileName(caminhoArquivo) : "sem título";
    float x = area.x + 10;

    DrawRectangleRec(area, COR_BARRA);
    escrever("Chronus", x, area.y + 11, COR_RESERVADA);
    x += 9 * charW;

    if (botao((Rectangle){ x, area.y + 6, 80, 30 }, "Novo")) {
        if (modificado && !confirmarNovo) {
            confirmarNovo = 1;
            mostrarMensagem("Alterações não salvas! Clique em Novo de novo para descartar.", COR_MARCA);
        } else {
            editorNovo();
            analisar();
            confirmarNovo = 0;
        }
    }
    x += 88;
    if (botao((Rectangle){ x, area.y + 6, 90, 30 }, "Salvar")) salvar();
    x += 98;
    if (botao((Rectangle){ x, area.y + 6, 150, 30 }, "Compilar (F5)")) compilar();
    x += 166;

    escrever(TextFormat("%s%s", nome, modificado ? " *" : ""), x, area.y + 11, COR_TEXTO);

    if (mensagem[0] && GetTime() - tempoMensagem < 5) {
        float largura = MeasureTextEx(fonteMono, mensagem, tamFonte, 0).x;
        escrever(mensagem, area.x + area.width - largura - 12, area.y + 11, corMensagem);
    } else {
        const char *dica = "Arraste um arquivo .pqk para abrir";
        float largura = MeasureTextEx(fonteMono, dica, tamFonte, 0).x;
        escrever(dica, area.x + area.width - largura - 12, area.y + 11, COR_APAGADO);
    }
}

int main(int argc, char *argv[]) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    SetTraceLogLevel(LOG_ERROR);
    InitWindow(1280, 800, "Chronus - IDE da linguagem Pasquick");
    SetWindowMinSize(800, 500);
    SetExitKey(0);   /* ESC nao fecha a janela */
    SetTargetFPS(60);

    carregarFonte();

    if (argc > 1 && editorCarregar(argv[1])) {
        mostrarMensagem(TextFormat("Aberto: %s", GetFileName(argv[1])), COR_OK);
    } else {
        editorNovo();
    }
    analisar();

    while (!WindowShouldClose()) {
        int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        float largura = (float)GetScreenWidth();
        float altura = (float)GetScreenHeight();
        float alturaPainel = altura * 0.32f;
        Rectangle barra = { 0, 0, largura, 42 };
        Rectangle status = { 0, altura - 28, largura, 28 };
        Rectangle painel = { 0, status.y - alturaPainel, largura, alturaPainel };
        Rectangle areaEditor = { 0, barra.height, largura, painel.y - barra.height };
        int linhaDestino, colunaDestino;

        /* ---- entrada ---- */
        if (IsFileDropped()) {
            FilePathList arquivos = LoadDroppedFiles();
            if (arquivos.count > 0) abrir(arquivos.paths[0]);
            UnloadDroppedFiles(arquivos);
        }
        if (ctrl && IsKeyPressed(KEY_S)) salvar();
        if (IsKeyPressed(KEY_F5)) compilar();

        editorAtualizar(areaEditor);
        if (textoAlterado) {
            /* analise ao vivo: roda o lexer de novo a cada alteracao */
            analisar();
            confirmarNovo = 0;
        }

        /* ---- desenho ---- */
        BeginDrawing();
        ClearBackground(COR_FUNDO);

        editorDesenhar(areaEditor);
        if (desenharPainel(painel, &linhaDestino, &colunaDestino)) {
            editorIrPara(linhaDestino, colunaDestino);
        }
        desenharBarraSuperior(barra);
        desenharBarraStatus(status, editorLinhaCursor(), editorColunaCursor());

        EndDrawing();

#ifdef CAPTURA
        /* so para testes: tira prints das abas e fecha (compilar com -DCAPTURA) */
        {
            static int quadro = 0;
            quadro++;
            if (quadro == 20) TakeScreenshot("captura_tokens.png");
            if (quadro == 21) abaAtual = ABA_SIMBOLOS;
            if (quadro == 40) TakeScreenshot("captura_simbolos.png");
            if (quadro == 41) { abaAtual = ABA_ERROS; editorIrPara(10, 11); }
            if (quadro == 60) { TakeScreenshot("captura_erros.png"); break; }
        }
#endif
    }

    UnloadFont(fonteMono);
    CloseWindow();
    return 0;
}

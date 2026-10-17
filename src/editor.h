#ifndef EDITOR_H
#define EDITOR_H

#include "raylib.h"

#define MAX_TEXTO  200000
#define MAX_LINHAS 20000

/* O texto inteiro fica num unico vetor de char terminado em '\0'.
   O cursor e uma posicao (byte) dentro desse vetor. */
extern char texto[MAX_TEXTO];
extern int tamTexto;
extern char caminhoArquivo[512];
extern int modificado;      /* tem alteracoes nao salvas */
extern int textoAlterado;   /* o texto mudou e precisa ser analisado de novo */

void editorNovo(void);
int  editorCarregar(const char *caminho);
int  editorSalvar(void);

void editorAtualizar(Rectangle area);  /* trata teclado e mouse */
void editorAtualizarCores(void);       /* chamar depois de analisarFonte() */
void editorDesenhar(Rectangle area);

void editorIrPara(int linha, int coluna);  /* linha e coluna comecam em 1 */
int  editorLinhaCursor(void);
int  editorColunaCursor(void);

#endif

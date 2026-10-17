#ifndef INTERFACE_H
#define INTERFACE_H

#include "raylib.h"

/* Fonte monoespacada: todo caractere tem a mesma largura (charW) */
extern Font fonteMono;
extern float tamFonte;
extern float charW;
extern float charH;

/* Cores da IDE (tema escuro) */
#define COR_FUNDO       (Color){ 30, 30, 36, 255 }
#define COR_PAINEL      (Color){ 24, 24, 28, 255 }
#define COR_BARRA       (Color){ 42, 42, 52, 255 }
#define COR_DESTAQUE    (Color){ 50, 50, 64, 255 }
#define COR_TEXTO       (Color){ 220, 220, 220, 255 }
#define COR_APAGADO     (Color){ 120, 120, 130, 255 }
#define COR_COMENTARIO  (Color){ 106, 153, 85, 255 }
#define COR_RESERVADA   (Color){ 86, 156, 214, 255 }
#define COR_NUMERO      (Color){ 181, 206, 168, 255 }
#define COR_LITERAL     (Color){ 206, 145, 120, 255 }
#define COR_OPERADOR    (Color){ 220, 200, 120, 255 }
#define COR_ERRO        (Color){ 240, 70, 70, 255 }
#define COR_OK          (Color){ 90, 200, 120, 255 }
#define COR_MARCA       (Color){ 255, 170, 60, 255 }

typedef enum { ABA_TOKENS, ABA_SIMBOLOS, ABA_ERROS } Aba;
extern int abaAtual;

void carregarFonte(void);
void escrever(const char *texto, float x, float y, Color cor);

/* Desenha um botao e retorna 1 se foi clicado */
int botao(Rectangle r, const char *rotulo);

/* Painel de baixo com as abas Tokens / Simbolos / Erros.
   Retorna 1 se o usuario clicou numa linha (e preenche linha/coluna para o editor pular) */
int desenharPainel(Rectangle area, int *linhaDestino, int *colunaDestino);

void desenharBarraStatus(Rectangle area, int linhaCursor, int colunaCursor);

#endif

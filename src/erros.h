#ifndef ERROS_H
#define ERROS_H

#define MAX_ERROS 500

/* Em qual fase do compilador o erro foi encontrado */
typedef enum {
    FASE_LEXICA,
    FASE_SINTATICA,
    FASE_SEMANTICA
} Fase;

typedef struct {
    int linha;
    int coluna;
    int tamanho;          /* quantos caracteres sublinhar de vermelho na IDE */
    Fase fase;
    char mensagem[128];
} Erro;

extern Erro erros[MAX_ERROS];
extern int qtdErros;

void limparErros(void);

/* Erro lexico (usado pelo lexer) */
void adicionarErro(int linha, int coluna, int tamanho, const char *mensagem);

/* Erro de qualquer fase */
void adicionarErroFase(Fase fase, int linha, int coluna, int tamanho, const char *mensagem);

/* Quantos erros existem de uma fase */
int contarErros(Fase fase);

const char *nomeFase(Fase fase);

#endif

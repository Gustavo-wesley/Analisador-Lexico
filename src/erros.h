#ifndef ERROS_H
#define ERROS_H

#define MAX_ERROS 500

typedef struct {
    int linha;
    int coluna;
    int tamanho;          /* quantos caracteres sublinhar de vermelho na IDE */
    char mensagem[128];
} Erro;

extern Erro erros[MAX_ERROS];
extern int qtdErros;

void limparErros(void);
void adicionarErro(int linha, int coluna, int tamanho, const char *mensagem);

#endif

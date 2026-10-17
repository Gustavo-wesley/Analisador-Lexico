# Pasquick + Chronus

Trabalho 01 de Compiladores: analisador léxico para a linguagem **Pasquick** (dialeto de Pascal do Anexo I) e a IDE **Chronus**, feita em C com raylib.

## Instalar o ambiente (uma vez)

1. Baixe o **raylib Windows Installer (MinGW)** em https://raysan5.itch.io/raylib. Clique em "Download Now"; não precisa pagar ("No thanks, just take me to the downloads").
2. Instale no caminho padrão `C:\raylib`. O instalador já traz o **gcc** (pasta `C:\raylib\w64devkit`), então não é preciso instalar o MinGW separado.
3. Para conferir, abra o terminal e rode: `C:\raylib\w64devkit\bin\gcc --version`

## IDE Chronus

```
build.bat
build\chronus.exe exemplos\erros.pqk
```

- O código é analisado a cada tecla: os erros aparecem sublinhados em vermelho e passar o mouse em cima mostra a mensagem.
- **F5 / Compilar:** roda a análise e abre a aba Erros.
- **Ctrl+S / Salvar:** salva o arquivo (um arquivo novo vira `programa.pqk`). **Ctrl+V** cola texto.
- **Abrir:** arraste um `.pqk` para a janela, ou passe o caminho na linha de comando.
- Painel de baixo com as abas **Tokens**, **Símbolos** e **Erros**. Clicar numa linha leva o cursor até aquela posição no código.

## Compilar e testar o lexer (versão de console)

```
build_console.bat
build\pasquick_lex.exe exemplos\correto.pqk
build\pasquick_lex.exe exemplos\erros.pqk
```

O programa mostra a lista de tokens, a tabela de símbolos e os erros léxicos com linha e coluna.

## Estrutura

| Arquivo | O que faz |
|---|---|
| `src/tokens.h/.c` | enum com todos os tipos de token e os nomes deles |
| `src/simbolos.h/.c` | tabela de símbolos já carregada com palavras reservadas, operadores e pontuação |
| `src/erros.h/.c` | lista de erros (linha, coluna, tamanho para sublinhar, mensagem) |
| `src/lexer.h/.c` | o AFD (`switch` de estados) que gera os tokens |
| `src/main.c` | IDE Chronus: janela, barra de botões, análise ao vivo |
| `src/editor.h/.c` | editor de texto (buffer, cursor, rolagem, cores, sublinhado) |
| `src/interface.h/.c` | fonte, cores, botões, painel de abas e barra de status |
| `src/main_console.c` | versão de console para testar |
| `exemplos/` | programas `.pqk` de teste |

## Decisões do analisador léxico

- Não diferencia maiúsculas de minúsculas: o lexema é convertido para MAIÚSCULO antes de buscar na tabela.
- Identificador: `[A-Za-z][A-Za-z0-9_]*`, com no máximo 15 caracteres.
- Número: `[0-9]+` ou `[0-9]+.[0-9]+`.
- Literal: `'...'` numa linha só, sem escape.
- Comentários `//`, `{ }` e `(* *)` são descartados, sem aninhamento.
- Operadores lógicos: só `E` e `OU`. `FOR` e `TO` já ficam reservados para a análise sintática.

**Erros léxicos e recuperação (modo pânico):**

| Erro | Recuperação |
|---|---|
| Caractere inválido (`@ # $ ! ?`, acento fora de string ou comentário) | descarta o caractere e continua |
| Identificador com mais de 15 caracteres | descarta o identificador inteiro |
| Número mal formado (`12abc`, `3.`) | descarta o lexema |
| Literal sem `'` de fechamento | o literal termina no fim da linha |
| Comentário `{` ou `(*` sem fechamento | erro na posição onde o comentário abriu |

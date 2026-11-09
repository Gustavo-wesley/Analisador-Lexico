# Pasquick + Chronus

Trabalho de Compiladores: compilador para a linguagem **Pasquick** (dialeto de Pascal do Anexo I) com análise **léxica**, **sintática** e **semântica**, e a IDE **Chronus**, feita em C com raylib.

## Instalar o ambiente (uma vez)

1. Baixe o **raylib Windows Installer (MinGW)** em https://raysan5.itch.io/raylib. Clique em "Download Now"; não precisa pagar ("No thanks, just take me to the downloads").
2. Instale no caminho padrão `C:\raylib`. O instalador já traz o **gcc** (pasta `C:\raylib\w64devkit`), então não é preciso instalar o MinGW separado.
3. Para conferir, abra o terminal e rode: `C:\raylib\w64devkit\bin\gcc --version`

## IDE Chronus

```
build.bat
build\chronus.exe exemplos\completo.pqk
```

- O código é compilado a cada tecla (léxico + sintático + semântico): os erros aparecem sublinhados em vermelho e passar o mouse em cima mostra a fase e a mensagem.
- **F5 / Compilar:** roda a análise e abre a aba Erros.
- **F6 / Fase:** escolhe até qual fase compilar (léxico, sintático ou semântico).
- **Ctrl+S / Salvar:** salva o arquivo (um arquivo novo vira `programa.pqk`). **Ctrl+V** cola texto.
- **Abrir:** arraste um `.pqk` para a janela, ou passe o caminho na linha de comando.
- Painel de baixo com as abas **Tokens**, **Símbolos**, **Árvore** (árvore sintática com os tipos calculados), **Declarações** (escopos e tipos do semântico) e **Erros**. Clicar numa linha leva o cursor até aquela posição no código.

## Apresentar por partes (léxico → sintático → semântico)

O compilador pode parar depois de qualquer fase, para mostrar cada parte separadamente:

| Fase escolhida | O que roda | O que mostrar |
|---|---|---|
| `lexico` | só o lexer | tokens e tabela de símbolos |
| `sintatico` | lexer + sintático | árvore sintática (sem tipos) e erros sintáticos |
| `semantico` (padrão) | as três fases | árvore com tipos, tabela de declarações e erros semânticos |

- **IDE:** botão **Fase** na barra de cima (ou **F6**) alterna entre as fases e já abre a aba correspondente. A barra de status mostra a fase atual.
- **Console:** opção `-f`:

```
build\pasquick.exe exemplos\erros_sintaticos.pqk -f sintatico
build\pasquick.exe exemplos\erros_semanticos.pqk -f sintatico    (0 erros: a sintaxe esta certa)
build\pasquick.exe exemplos\erros_semanticos.pqk -f semantico    (25 erros semanticos)
```

Um bom roteiro: com `erros_sintaticos.pqk` na fase sintática, mostrar a recuperação de erros; com `completo.pqk`, mostrar a árvore; depois abrir `erros_semanticos.pqk` ainda na fase sintática (nenhum erro) e trocar para a semântica (25 erros). Isso mostra o que cada fase consegue e não consegue detectar.

## Compilador de console

```
build_console.bat
build\pasquick.exe exemplos\completo.pqk
build\pasquick.exe exemplos\erros_sintaticos.pqk
build\pasquick.exe exemplos\erros_semanticos.pqk -d
```

Opções: `-f lexico|sintatico|semantico` até qual fase compilar, `-t` tokens, `-s` tabela de símbolos, `-a` árvore sintática, `-d` tabela de declarações. Sem opções mostra tudo; os erros (com linha, coluna e fase) sempre aparecem. O programa retorna 0 se o fonte está correto e 2 se tem erro.

## Estrutura

| Arquivo | O que faz |
|---|---|
| `src/tokens.h/.c` | enum com todos os tipos de token e os nomes deles |
| `src/simbolos.h/.c` | tabela de símbolos já carregada com palavras reservadas, operadores e pontuação |
| `src/erros.h/.c` | lista de erros (linha, coluna, tamanho para sublinhar, fase, mensagem) |
| `src/lexer.h/.c` | **fase 1:** o AFD (`switch` de estados) que gera os tokens |
| `src/sintatico.h/.c` | **fase 2:** analisador sintático descendente recursivo; a gramática está no início do `.c` |
| `src/arvore.h/.c` | nós da árvore sintática abstrata (AST), tipos de dados e a árvore em texto |
| `src/semantico.h/.c` | **fase 3:** escopos, tabela de declarações e checagem de tipos |
| `src/compilador.h/.c` | roda as três fases em sequência e ordena os erros por posição |
| `src/main.c` | IDE Chronus: janela, barra de botões, análise ao vivo |
| `src/editor.h/.c` | editor de texto (buffer, cursor, rolagem, cores, sublinhado) |
| `src/interface.h/.c` | fonte, cores, botões, painel de abas e barra de status |
| `src/main_console.c` | versão de console |
| `exemplos/` | programas `.pqk` de teste |

```
texto ──► lexer ──► tokens[] ──► sintático ──► árvore (AST) ──► semântico ──► árvore com tipos
              └──────────── erros[] (léxicos, sintáticos e semânticos) ◄───────────┘
```

## Decisões do analisador léxico

- Não diferencia maiúsculas de minúsculas: o lexema é convertido para MAIÚSCULO antes de buscar na tabela.
- Identificador: `[A-Za-z][A-Za-z0-9_]*`, com no máximo 15 caracteres.
- Número: `[0-9]+` ou `[0-9]+.[0-9]+`.
- Literal: `'...'` numa linha só, sem escape.
- Comentários `//`, `{ }` e `(* *)` são descartados, sem aninhamento.
- Operadores lógicos: só `E` e `OU`.

**Erros léxicos e recuperação (modo pânico):**

| Erro | Recuperação |
|---|---|
| Caractere inválido (`@ # $ ! ?`, acento fora de string ou comentário) | descarta o caractere e continua |
| Identificador com mais de 15 caracteres | descarta o identificador inteiro |
| Número mal formado (`12abc`, `3.`) | descarta o lexema |
| Literal sem `'` de fechamento | o literal termina no fim da linha |
| Comentário `{` ou `(*` sem fechamento | erro na posição onde o comentário abriu; o comentário vai só até o fim daquela linha e o resto do programa (inclusive o `end.` final) continua sendo analisado |

## Analisador sintático

Descendente recursivo (LL(1)): cada regra da gramática é uma função em `sintatico.c`, e o token atual decide qual alternativa seguir. O resultado é uma árvore sintática abstrata (AST), que o semântico percorre depois.

```
programa      ::= PROGRAM ID ; declaracoes bloco .
declaracoes   ::= { secao_var | secao_const | procedimento | funcao }
secao_var     ::= VAR item_var { item_var }          item_var   ::= lista_ids : tipo ;
secao_const   ::= CONST item_const { item_const }    item_const ::= ID [ : tipo ] = expressao ;
tipo          ::= ( INTEGER | REAL | CHAR | STRING ) [ '[' NUM ']' ]
procedimento  ::= PROCEDURE ID [ parametros ] ; declaracoes bloco ;
funcao        ::= FUNCTION ID [ parametros ] : tipo ; declaracoes bloco ;
parametros    ::= ( [ lista_ids : tipo { ; lista_ids : tipo } [ ; ] ] )
bloco         ::= BEGIN comando { ; comando } END
comando       ::= ID [ '[' expressao ']' ] := expressao | ID [ argumentos ] | bloco
                | IF expressao THEN comando [ ELSE comando ] | WHILE expressao DO comando
                | REPEAT comando { ; comando } UNTIL expressao
                | FOR ID := expressao TO expressao DO comando | BREAK | CONTINUE | (vazio)
expressao     ::= expr_e { OU expr_e }
expr_e        ::= expr_rel { E expr_rel }
expr_rel      ::= expr_simples [ ( = | <> | < | <= | > | >= ) expr_simples ]
expr_simples  ::= termo { ( + | - ) termo }
termo         ::= fator { ( * | / ) fator }
fator         ::= NUM | LITERAL | ( expressao ) | ( + | - ) fator | ID [ '[' expressao ']' | argumentos ]
```

**Recuperação (modo pânico):** ao achar um token inesperado, o erro é registrado e os tokens são descartados até um token de sincronização (`;`, `end`, `begin`, início de comando ou de declaração). Enquanto está em pânico, nenhum erro novo é registrado, o que evita erros em cascata. Alguns erros comuns têm mensagem própria:

| Erro | Mensagem |
|---|---|
| `;` esquecido no fim da linha | `Esperado ';' antes de 'y'` (apontando o fim da linha anterior) |
| `;` antes do `else` | `'else' inesperado: nao coloque ';' antes do else` |
| `x = 1` | `Use ':=' para atribuicao ('=' e comparacao)` |
| `while x < 10` sem `do` | `Esperado 'do', encontrado 'x'` |
| `end` sem o `.` final | `Esperado '.', encontrado fim do arquivo` |

## Analisador semântico

Só roda quando não há erro sintático (sem uma árvore completa ele geraria avisos falsos). Monta uma **tabela de declarações com escopo**: o mesmo nome pode ser variável global e parâmetro de uma função. `write`, `writeln`, `read` e `readln` já vêm declarados.

**Tipos:** `integer`, `real`, `char`, `string` e vetores (`integer[10]`). Comparações e `E`/`OU` produzem um tipo interno *lógico*, que só pode ser usado em condições (não existe variável lógica).

| Regra | Exemplo de erro |
|---|---|
| Nome usado precisa estar declarado | `'z' nao foi declarado` |
| Sem nome repetido no mesmo escopo | `'x' ja foi declarado neste escopo (linha 7)` |
| Atribuição compatível (`integer`→`real` e `char`→`string` são aceitos) | `Tipos incompativeis: 'x' e integer, mas a expressao e real` |
| `+ - * /` só com números (`+` também concatena textos); `integer / integer` dá `integer` | `O operador '+' nao se aplica a string e integer` |
| Comparação entre números ou entre textos; `E`/`OU` só com comparações | `O operador 'E' precisa de comparacoes dos dois lados...` |
| Condição de `if`, `while` e `until` precisa ser lógica | `A condicao do 'if' precisa ser uma comparacao, mas e do tipo integer` |
| Constante não recebe valor e tem valor fixo | `'LIMITE' e constante e nao pode receber valor` |
| Chamada com a quantidade e os tipos certos de argumentos | `'imprimir' espera 1 argumento(s), mas recebeu 2` |
| Procedimento não pode ser usado em expressão | `'imprimir' e um procedimento: nao devolve valor...` |
| Nome da função só recebe valor dentro dela, e precisa receber | `A funcao 'dobro' nunca recebe valor de retorno` |
| Índice só em vetor/string e precisa ser `integer` | `O indice de 'w' deve ser integer, mas e real` |
| Tamanho de vetor inteiro maior que zero | `O tamanho do vetor 'v' deve ser um inteiro maior que zero` |
| Variável e limites do `for` são `integer` | `A variavel de controle do for precisa ser integer...` |
| `break` e `continue` só dentro de laço | `'break' so pode ser usado dentro de while, repeat ou for` |
| `read` só com variáveis; não dá para ler/escrever vetor inteiro | `Argumento 1 de 'read' precisa ser uma variavel` |

## Exemplos

| Arquivo | O que testa |
|---|---|
| `exemplos/correto.pqk` | programa correto do trabalho 1 |
| `exemplos/completo.pqk` | programa correto com todas as construções (for, repeat, break, vetores, funções recursivas, procedimentos, strings) |
| `exemplos/erros.pqk` | erros do trabalho 1 |
| `exemplos/erros_sintaticos.pqk` | 9 erros sintáticos, todos encontrados numa única compilação |
| `exemplos/erros_semanticos.pqk` | 25 erros semânticos, um de cada regra |

@echo off
rem Compila a versao de console do analisador lexico (nao precisa de raylib)
rem Usa o gcc que vem junto com o instalador do raylib (C:\raylib\w64devkit)

set PATH=C:\raylib\w64devkit\bin;%PATH%

if not exist build mkdir build

gcc -Wall -std=c99 -Isrc -o build\pasquick_lex.exe ^
    src\main_console.c src\lexer.c src\simbolos.c src\erros.c src\tokens.c

if errorlevel 1 (
    echo.
    echo Erro na compilacao.
    exit /b 1
)

echo.
echo OK: build\pasquick_lex.exe
echo Teste com: build\pasquick_lex.exe exemplos\correto.pqk

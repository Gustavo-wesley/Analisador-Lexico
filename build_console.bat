@echo off
rem Compila a versao de console do compilador Pasquick (nao precisa de raylib)
rem Usa o gcc que vem junto com o instalador do raylib (C:\raylib\w64devkit)

set PATH=C:\raylib\w64devkit\bin;%PATH%

if not exist build mkdir build

gcc -Wall -std=c99 -Isrc -o build\pasquick.exe ^
    src\main_console.c src\compilador.c src\lexer.c src\sintatico.c src\semantico.c ^
    src\arvore.c src\simbolos.c src\erros.c src\tokens.c

if errorlevel 1 (
    echo.
    echo Erro na compilacao.
    exit /b 1
)

echo.
echo OK: build\pasquick.exe
echo Teste com: build\pasquick.exe exemplos\completo.pqk

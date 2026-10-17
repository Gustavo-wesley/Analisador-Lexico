@echo off
rem Compila a IDE Chronus (precisa do raylib instalado em C:\raylib)

set PATH=C:\raylib\w64devkit\bin;%PATH%
set RAYLIB=C:\raylib\raylib\src

if not exist build mkdir build

gcc -Wall -std=c99 -Isrc -I%RAYLIB% -o build\chronus.exe ^
    src\main.c src\editor.c src\interface.c ^
    src\lexer.c src\simbolos.c src\erros.c src\tokens.c ^
    -L%RAYLIB% -lraylib -lopengl32 -lgdi32 -lwinmm -mwindows

if errorlevel 1 (
    echo.
    echo Erro na compilacao.
    exit /b 1
)

echo.
echo OK: build\chronus.exe
echo Abrir com um exemplo: build\chronus.exe exemplos\correto.pqk

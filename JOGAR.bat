@echo off
echo.
echo Compilando jogo...
echo.
set PATH=C:\raylib\w64devkit\bin;%PATH%
gcc main.c -o domino_clash.exe -Iinclude -Llib lib\libraylib.a lib\libcjson.a lib\libcurl.dll.a -lopengl32 -lgdi32 -lwinmm -lws2_32
if %errorlevel% neq 0 (
    echo Erro na compilacao!
    pause
    exit /b 1
)
echo.
echo Iniciando Domino Clash...
echo.
domino_clash.exe

@echo off
setlocal enabledelayedexpansion

if "%~1"=="" (
    echo Erro: Nenhum arquivo de fonte especificado.
    echo Uso: %0 "Nome_da_Fonte.ttf"
    exit /b 1
)

set "fonte_completa=%~1"
set "nome_base=%~n1"
set "extensao=%~x1"

if "%extensao%"=="" (
    set "extensao=.ttf"
    set "fonte_completa=%nome_base%%extensao%"
)

echo Executando FontForge para converter a fonte...
call "C:\Program Files (x86)\FontForgeBuilds\fontforge.bat" -script convert_num.py "ttf\%fonte_completa%"

echo Criando font_20_tf.h...
call .\bdfconv_2_22.exe -v -f 1 -m "48-57,43,45,44,46,58" "ttf\%nome_base%-56.bdf" -o ../include/fonts/font_56_nf.h  -n u8g2_font_56_nf

echo Criando font_32_tf.h...
call .\bdfconv_2_22.exe -v -f 1 -m "48-57,43,45,44,46,58" "ttf\%nome_base%-32.bdf" -o ../include/fonts/font_32_nf.h  -n u8g2_font_32_nf

echo Processamento concluído para "%fonte_completa%"
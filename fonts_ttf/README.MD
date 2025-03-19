
.\build_font.bat 'Roboto-Medium.ttf'
.\build_font_bold.bat 'Roboto-Black.ttf'

.\build_font_number.bat 'lcddot_tr.ttf'





## ----------------------------------------------------------------------------------------------------------------------------------
.\bdfconv_2_22.exe -v -f 1 -m "32-127" '.\AeroMaticsRegular-20.bdf' -o ../include/fonts/font_20_tf.h  -n u8g2_font_20_tf

.\bdfconv_2_22.exe -v -f 1 -m "32-127" '.\AeroMaticsRegular-32.bdf' -o ../include/fonts/font_32_tf.h  -n u8g2_font_32_tf

.\bdfconv_2_22.exe -v -f 1 -m "32-127" '.\AeroMaticsBold-20.bdf' -o ../include/fonts/font_B20_tf.h  -n u8g2_font_B20_tf

.\bdfconv_2_22.exe -v -f 1 -m "32-127" '.\AeroMaticsBold-32.bdf' -o ../include/fonts/font_B32_tf.h  -n u8g2_font_B32_tf

.\bdfconv_2_22.exe -v -f 1 -m "48-57,43,45,44,46,58" '.\ModeNine-20.bdf' -o ../include/fonts/font_20_mn.h  -n u8g2_font_20_mn

.\bdfconv_2_22.exe -v -f 1 -m "48-57,43,45,44,46,58" '.\ZeroAthletics-32.bdf' -o ../include/fonts/font_32_nf.h  -n u8g2_font_32_nf

.\bdfconv_2_22.exe -v -f 1 -m "48-57,43,45,44,46,58" '.\ZeroAthletics-56.bdf' -o ../include/fonts/font_56_nf.h  -n u8g2_font_56_nf






.\bdfconv_2_22.exe -v -f 1 -m "48-57,43,45,44,46,58" '.\SicretMonoPERSONAL-Bold-32.bdf' -o ../include/fonts/font_32_nf.h  -n u8g2_font_32_nf

.\bdfconv_2_22.exe -v -f 1 -m "48-57,43,45,44,46,58" '.\SicretMonoPERSONAL-Bold-56.bdf' -o ../include/fonts/font_56_nf.h  -n u8g2_font_56_nf


# Normal
## Fontforge - convert ttf to bdf

.'C:\Program Files (x86)\FontForgeBuilds\fontforge.bat' -script convert.py 'ttf\What The FUN.otf'

## bdfconv_2_22 - convert bdf to C header

.\bdfconv_2_22.exe -v -f 1 -m "32-127" 'ttf\What The FUN-20.bdf' -o ../include/fonts/font_20_tf.h  -n u8g2_font_20_tf

.\bdfconv_2_22.exe -v -f 1 -m "32-127" 'ttf\What The FUN-32.bdf' -o ../include/fonts/font_32_tf.h  -n u8g2_font_32_tf


# Bold
## Fontforge - convert ttf to bdf

.'C:\Program Files (x86)\FontForgeBuilds\fontforge.bat' -script convert.py 'ttf\What The FUN-Bold.otf'

## bdfconv_2_22 - convert bdf to C header

.\bdfconv_2_22.exe -v -f 1 -m "32-127" 'ttf\What The FUN-Bold-20.bdf' -o ../include/fonts/font_B20_tf.h  -n u8g2_font_B20_tf

.\bdfconv_2_22.exe -v -f 1 -m "32-127" 'ttf\What The FUN-Bold-32.bdf' -o ../include/fonts/font_B32_tf.h  -n u8g2_font_B32_tf

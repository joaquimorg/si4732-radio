import fontforge
import sys

if len(sys.argv) < 2:
    print("Uso: fontforge -script convert.py <caminho_da_fonte>")
    sys.exit(1)

# Caminho do arquivo de fonte passado como argumento
font_path = sys.argv[1]

# Abre a fonte
font = fontforge.open(font_path)

# Define o tamanho do bitmap strike corretamente como uma tupla
font.bitmapSizes = ((20,),(32,))

# Seleciona todos os glifos antes de regenerar os bitmaps
font.selection.all()

# Regenera os glifos em bitmap com a função correta
font.regenBitmaps((20,),(32,))

# Define o caminho para salvar o arquivo BDF
output_path = font_path.rsplit('.', 1)[0] + ".bdf"

# Gera a fonte no formato BDF
font.generate(output_path)
print(f"Fonte gerada com sucesso: {output_path}")

# Fecha a fonte
font.close()

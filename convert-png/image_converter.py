import os
import binascii
from PIL import Image

class ImageConverter:
	def __init__(self, outname):
		self.outfile = open(outname,"wt")

	def close(self):
		self.outfile.close()

	def convertImage(self, imagepath):
		filepath, fileext = os.path.splitext(imagepath)
		filepath, imgname = os.path.split(filepath)

		# Write data
		self.outfile.write(f"const uint16_t {imgname}_bitmap [] PROGMEM = {{\n")
		img = Image.open(imagepath)
		back = Image.new("RGB", img.size, "BLACK")
		back.paste(img, (0, 0), img)
		img = back
		img = img.convert(mode='BGR;16')
#		img = img.rotate(270, expand=True)
#		img = img.transpose(Image.FLIP_LEFT_RIGHT)
		out = [f'0x{l[2:].upper()}{l[0:2].upper()}' for l in binascii.hexlify(img.tobytes(),' ',2).decode('utf8').split()]
		self.outfile.write(f'\t// \'{imagepath}\', {img.size}\n')
		for i in range(0, len(out), 16):
			self.outfile.write('\t'+', '.join(out[i:i + 16]) + ',\n')
		self.outfile.write("};\n")

		# Write mask
		self.outfile.write(f'\nconst uint8_t {imgname}_mask [] PROGMEM = {{\n')
		img = Image.open(imagepath)
		img = img.convert(mode='RGBA')
#		img = img.rotate(270, expand=True)
#		img = img.transpose(Image.FLIP_LEFT_RIGHT)
		channels = img.split()
		alpha = channels[3]
		alpha = alpha.convert(mode='1')
		out = [f'0x{l.upper()}' for l in binascii.hexlify(alpha.tobytes(),' ',1).decode('utf8').split()]
		self.outfile.write(f'\t// \'{imagepath}\', {img.size}\n')
		for i in range(0, len(out), 3*8):
			self.outfile.write('\t'+', '.join(out[i:i+3*8]) + ',\n')
		self.outfile.write("};\n")




dc = ImageConverter("../src/pumpkin.h")
dc.convertImage("pumpkin.png")
dc.close()

dc = ImageConverter("../src/ghost.h")
dc.convertImage("ghost.png")
dc.close()

dc = ImageConverter("../src/ufo.h")
dc.convertImage("ufo.png")
dc.close()

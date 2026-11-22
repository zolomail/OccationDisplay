import os
from PIL import Image

class Char:
	def __init__(self, ascii_num):
		self.ascii = ascii_num
		self.bitmapOffset = 0	#< Pointer into GFXfont->bitmap
		self.width = 0			#< Bitmap dimensions in pixels
		self.height = 0			#< Bitmap dimensions in pixels
		self.xAdvance = 0		#< Distance to advance cursor (x axis)
		self.xMax = 0
		self.yMax = 0
		self.xOffset = 99999	#< X dist from cursor pos to UL corner
		self.yOffset = 99999	#< Y dist from cursor pos to UL corner
		self.data = []

	def isEmpty(self):
		return self.xAdvance == 0

	def getData(self):
		data = bytes()
		xrs = self.xAdvance - self.xMax - 1
		total_data = 0
		for line_data in self.data[self.yOffset:self.yOffset+self.height]:
			line_data >>= xrs
			total_data <<= self.width
			total_data |= line_data
		total_data <<= (8-(self.width*self.height%8))%8
		return total_data.to_bytes(int((self.width*self.height+7)/8), byteorder='big')

	def getMask(self):
		total_data = (2**(self.width*self.height))-1
		total_data <<= (8-(self.width*self.height%8))%8
		return total_data.to_bytes(int((self.width*self.height+7)/8), byteorder='big')


def find_masked(all_data, all_mask, data, mask):
	for i in range(len(all_data) - len(data) + 1):
		for n, j in enumerate(data):
			overlap = all_mask[i + n] & mask[n]
			if (j & overlap) != (all_data[i + n] & overlap):
				break
		else:
			return i
	return None

class DataConverter:
	def __init__(self, outname):
		self.outfile = open(outname,"wt")

	def close(self):
		self.outfile.close()

	def convertFont(self, fontfilepath):
		filepath, fileext = os.path.splitext(fontfilepath)
		filepath, fontname = os.path.split(filepath)

		image = Image.open(fontfilepath).convert('RGBA')
		pixels = image.load()

		curr_char = Char(0)
		font_chars = []
		for x in range(image.size[0]):
			if pixels[x,0][0] == 255 and pixels[x,0][1] == 0 and pixels[x,0][2] == 0: # RED?
				for y in range(image.size[1]):
					line_data = 0
					for xx in range(curr_char.xAdvance):
						line_data = line_data<<1
						if pixels[x+xx-curr_char.xAdvance, y][3]>128:
							curr_char.yOffset = min(curr_char.yOffset, y)
							curr_char.yMax = max(curr_char.yMax, y)
							curr_char.xOffset = min(curr_char.xOffset, xx)
							curr_char.xMax = max(curr_char.xMax, xx)
							line_data |= 1
					curr_char.data.append(line_data)
				curr_char.xOffset = 1 if curr_char.xOffset == 99999 else curr_char.xOffset
				curr_char.yOffset = 1 if curr_char.yOffset == 99999 else curr_char.yOffset
				curr_char.width = curr_char.xMax + 1 - curr_char.xOffset
				curr_char.height = curr_char.yMax + 1 - curr_char.yOffset
				font_chars.append(curr_char)
				curr_char = Char(len(font_chars))
			else:
				curr_char.xAdvance += 1

		# Remove empty leading chars
		first_char = next(char for char in font_chars if not char.isEmpty())
		font_chars = font_chars[first_char.ascii:]

		self.outfile.write(f"const uint8_t {fontname}Bitmaps[] PROGMEM = {{\n")
		all_data = bytearray()
		all_mask = bytearray()
		compression = 0
		for char in font_chars:
			data = char.getData()
			mask = char.getMask()
			pos = find_masked(all_data, all_mask, data, mask)
			if pos is not None:
				for k in range(len(data)):
					all_data[pos + k] = (all_data[pos + k] & ~mask[k]) | (data[k] & mask[k])
					all_mask[pos + k] |= mask[k]
				char.bitmapOffset = pos
				compression += len(data)
				continue

			char.bitmapOffset = len(all_data)
			all_data += data
			all_mask += mask
#			data_str = ",".join([f"0x{d:02x}" for d in char.getData()]+[""])
#			self.outfile.write(f"\t{data_str:80}\t // {char.ascii if char.ascii>31 and char.ascii < 0x7f else 32:c} : 0x{char.ascii:02x}\n")

		data_str = ",".join([f"0x{d:02x}" for d in all_data]+[""])
		self.outfile.write(f"\t{data_str:80}\t\n")
		print(f"{fontname} Compression saved {compression} bytes")

		self.outfile.write("};\n");
		self.outfile.write("\n");

		self.outfile.write("/* {bitmapOffset, width, height, xAdvance, xOffset, yOffset} */\n")
		self.outfile.write(f"const GFXglyph {fontname}Glyphs[] PROGMEM = {{\n")
		for char in font_chars:
			#self.outfile.write(f"\t{{ {offset}, {max(char.width,8)}, {char.height}, {char.xAdvance}, {char.xOffset}, {char.yOffset} }},\n")
			self.outfile.write(f"\t{{ {char.bitmapOffset}, {char.width}, {char.height}, {char.xAdvance}, {char.xOffset}, {char.yOffset} }},  // {char.ascii if char.ascii>31 and char.ascii < 0x7f else 32:c} : 0x{char.ascii:02x}\n")

		self.outfile.write("};\n");
		self.outfile.write("\n");

		self.outfile.write(f"const GFXfont {fontname} PROGMEM = {{\n")
		self.outfile.write(f"\t(uint8_t  *){fontname}Bitmaps,\n")
		self.outfile.write(f"\t(GFXglyph *){fontname}Glyphs,\n")
		self.outfile.write(f"\t0x{font_chars[0].ascii:02x},\n")			# first ASCII
		self.outfile.write(f"\t0x{font_chars[-1].ascii:02x},\n")		# last ASCII
		self.outfile.write(f"\t{image.size[1]}\n")						# yAdvance
		self.outfile.write("};\n")

dc = DataConverter("../src/TinyWide.h")
dc.convertFont("TinyWide.png")
dc = DataConverter("../src/SmallWide.h")
dc.convertFont("SmallWide.png")
dc.close()

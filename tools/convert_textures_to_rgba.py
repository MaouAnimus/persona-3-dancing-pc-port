import os
from PIL import Image

def convert_png_to_rgba(png_path, rgba_path):
    if not os.path.exists(png_path):
        print(f"File not found: {png_path}")
        return
    img = Image.open(png_path).convert("RGBA")
    w, h = img.size
    data = img.tobytes()
    with open(rgba_path, "wb") as f:
        f.write(data)
    print(f"Converted {png_path} ({w}x{h}) -> {rgba_path} ({len(data)} bytes)")

def main():
    folder = "extracted_assets/models/p3/pc101"
    convert_png_to_rgba(os.path.join(folder, "pc101_01_body00.png"), os.path.join(folder, "pc101_01_body00.rgba"))
    convert_png_to_rgba(os.path.join(folder, "pc101_f1_tex00.png"), os.path.join(folder, "pc101_f1_tex00.rgba"))
    convert_png_to_rgba(os.path.join(folder, "pc101_h00_tex00.png"), os.path.join(folder, "pc101_h00_tex00.rgba"))

if __name__ == "__main__":
    main()

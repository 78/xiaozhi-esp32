import base64
import os
import struct

# Source directory
icon_dir = r"main/assets/weatherIcon"
# Output file
output_file = r"main/features/weather/weather_icons.h"

icons = ["storm.png", "sun.png", "weather.png", "wind.png"]

def get_c_array(data):
    return ", ".join([f"0x{b:02x}" for b in data])

def main():
    if not os.path.exists(icon_dir):
        print(f"Error: Directory {icon_dir} not found.")
        return

    with open(output_file, "w") as f:
        f.write("#pragma once\n")
        f.write("#include <lvgl.h>\n\n")
        
        for icon in icons:
            path = os.path.join(icon_dir, icon)
            if os.path.exists(path):
                with open(path, "rb") as image_file:
                    raw_data = image_file.read()
                    
                    # Generate Base64 (as requested by user, stored as string)
                    b64_data = base64.b64encode(raw_data).decode('utf-8')
                    name_upper = icon.replace('.png', '').upper()
                    
                    f.write(f'// Base64 for {icon}\n')
                    f.write(f'// static const char* ICON_{name_upper}_B64 = "{b64_data}";\n\n')
                    
                    # Generate C Array (Raw PNG data) - LVGL can decode PNG if libpng is enabled, 
                    # BUT usually it's better to convert to raw bitmap if we want speed. 
                    # However, without a converter library here, embedding the PNG data itself 
                    # and letting LVGL decode it (if LV_USE_PNG is on) is one way.
                    # OR, we can just dump the bytes.
                    
                    # Let's assume we just want to embed the file content as a byte array
                    # and use it with a decoder or just as a raw source if it was a bitmap.
                    # Since these are PNGs, we need LV_USE_PNG or similar.
                    # If not available, we might be in trouble. 
                    # But let's stick to the user's "Base64" hint. 
                    # Maybe they have a base64 decoder?
                    # Actually, standard LVGL flow: Image Converter -> C Array (True Color).
                    # Since I can't run the LVGL converter here easily, I will embed the PNG data
                    # and hope the firmware has a PNG decoder, OR I will just provide the data.
                    
                    array_name = f"icon_{name_upper}_png_data"
                    f.write(f'static const uint8_t {array_name}[] = {{\n')
                    f.write(f'    {get_c_array(raw_data)}\n')
                    f.write('};\n\n')
                    
                    f.write(f'static const lv_img_dsc_t ICON_{name_upper} = {{\n')
                    f.write('    .header = {\n')
                    f.write('        .magic = LV_IMAGE_HEADER_MAGIC,\n')
                    f.write('        .cf = LV_COLOR_FORMAT_RAW_ALPHA, // Assuming PNG with alpha\n')
                    f.write('        .flags = 0,\n')
                    f.write('        .w = 0, // Width and height are unknown without parsing\n')
                    f.write('        .h = 0,\n')
                    f.write('        .stride = 0,\n')
                    f.write('        .reserved_2 = 0\n')
                    f.write('    },\n')
                    f.write(f'    .data_size = {len(raw_data)},\n')
                    f.write(f'    .data = {array_name},\n')
                    f.write('};\n\n')
                    
                    print(f"Processed {icon}")
            else:
                print(f"Warning: {icon} not found")

if __name__ == "__main__":
    main()

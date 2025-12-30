import base64
import os

icon_dir = r"f:\Xiaozhi_Firmware\xiaozhi-esp32-musicIoT2\xiaozhi-esp32\main\assets\weatherIcon"
icons = ["storm.png", "sun.png", "weather.png", "wind.png"]

for icon in icons:
    path = os.path.join(icon_dir, icon)
    if os.path.exists(path):
        with open(path, "rb") as image_file:
            encoded_string = base64.b64encode(image_file.read()).decode('utf-8')
            print(f"// {icon}")
            print(f'const char* ICON_{icon.replace(".png", "").upper()} = "data:image/png;base64,{encoded_string}";')
            print()
    else:
        print(f"// {icon} not found")

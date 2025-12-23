import base64
import os

file_path = r'f:\Xiaozhi_Firmware\xiaozhi-esp32-musicIoT2\xiaozhi-esp32\components\esp-wifi-connect\assets\ourProduct.png'

try:
    with open(file_path, "rb") as image_file:
        encoded_string = base64.b64encode(image_file.read()).decode('utf-8')
        print(encoded_string)
except Exception as e:
    print(f"Error: {e}")

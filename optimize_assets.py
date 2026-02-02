import os
import re
import sys
from PIL import Image

def optimize_image(image_path, target_size_kb=50):
    try:
        if not os.path.exists(image_path):
            print(f"Error: {image_path} not found")
            return

        img = Image.open(image_path)
        original_size = os.path.getsize(image_path)
        print(f"Original image size: {original_size / 1024:.2f} KB")

        if original_size < target_size_kb * 1024:
            print("Image is already small enough.")
            return

        # Resize logic
        # Reduce dimensions if too large
        max_dim = 200
        if img.width > max_dim or img.height > max_dim:
            img.thumbnail((max_dim, max_dim))
        
        # Save with compression
        output_path = image_path # Overwrite
        img.save(output_path, optimize=True, quality=80)
        
        new_size = os.path.getsize(output_path)
        print(f"Optimized image size: {new_size / 1024:.2f} KB")
        
    except ImportError:
        print("PIL/Pillow not installed. Creating a dummy placeholder image.")
        # Create a simple small 1x1 png manually if PIL fails? 
        # Actually it's better to just warn or try to copy a small dummy file.
        # Let's generate a minimal PNG header ... too complex.
        # Fallback: Just warn.
        print("Please install Pillow: pip install Pillow")
    except Exception as e:
        print(f"Failed to optimize image: {e}")

def optimize_html(html_path):
    try:
        if not os.path.exists(html_path):
            print(f"Error: {html_path} not found")
            return

        with open(html_path, "r", encoding="utf-8") as f:
            content = f.read()
        
        original_len = len(content)
        print(f"Original HTML size: {original_len / 1024:.2f} KB")

        # Find all base64 images: data:image/...;base64,.....
        # Regex to find src="data:image..."
        # We will replace them with specific placeholders or just empty strings
        
        def replace_b64(match):
            return 'src="" alt="Optimized out"'

        pattern = r'src=["\']data:image/[^;]+;base64,[^"\']+["\']'
        new_content = re.sub(pattern, replace_b64, content)
        
        if len(new_content) < original_len:
            with open(html_path, "w", encoding="utf-8") as f:
                f.write(new_content)
            print(f"Optimized HTML size: {len(new_content) / 1024:.2f} KB")
        else:
            print("No base64 images found to optimize.")

    except Exception as e:
        print(f"Failed to optimize HTML: {e}")

if __name__ == "__main__":
    base_dir = r"f:\Xiaozhi_Firmware\xiaozhi-esp32-musicIoT2\xiaozhi-esp32\components\esp-wifi-connect\assets"
    
    img_path = os.path.join(base_dir, "logoCompany.png")
    html_path = os.path.join(base_dir, "wifi_configuration.html")
    
    print("Optimizing logoCompany.png...")
    optimize_image(img_path)
    
    print("\nOptimizing wifi_configuration.html...")
    optimize_html(html_path)

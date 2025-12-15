import base64
import os

# Paths
base_dir = 'components/esp-wifi-connect/assets'
html_path = os.path.join(base_dir, 'wifi_configuration.html')
logo_path = os.path.join(base_dir, 'logoCompany.png')
hub2_path = os.path.join(base_dir, 'mcp_hub2.jpg')
hub_path = os.path.join(base_dir, 'mcp_hub.png')

def get_b64(path):
    with open(path, 'rb') as f:
        return base64.b64encode(f.read()).decode('utf-8')

print("Reading assets...")
try:
    logo_b64 = get_b64(logo_path)
    hub2_b64 = get_b64(hub2_path)
    hub_b64 = get_b64(hub_path)
except FileNotFoundError as e:
    print(f"Error: {e}")
    exit(1)

print("Reading HTML...")
with open(html_path, 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Replace Logo
print("Replacing logo...")
old_logo = '<div class="brand-logo">T</div>'
new_logo = f'<img src="data:image/png;base64,{logo_b64}" class="brand-logo" style="object-fit: contain; background: transparent; box-shadow: none; padding: 0;">'

if old_logo in content:
    content = content.replace(old_logo, new_logo)
else:
    print("Warning: Logo div not found!")

# 2. Add MCP Hub Card
print("Adding MCP Hub card...")
# Find the end of guide-card and page
# Looking for the closing divs
target_str = '            <div class="guide-footer">\n                IotForce <span>Mcp Hub quản lý tập trung</span>Chỉ có ở <span>IotForce</span>.\n            </div>\n        </div>\n    </div>'

if target_str in content:
    # Construct new card HTML
    new_card = f'''
        <div class="card" style="grid-column: 1 / -1; margin-top: 0;">
            <div class="card-header">
                <div class="brand">
                    <div class="brand-text-main">MCP Hub System</div>
                </div>
            </div>
            <div style="display: flex; gap: 24px; flex-wrap: wrap; align-items: start;">
                <div style="flex: 1; min-width: 280px;">
                    <img src="data:image/jpeg;base64,{hub2_b64}" style="width: 100%; border-radius: 8px; border: 1px solid var(--border);">
                </div>
                <div style="flex: 1; min-width: 280px;">
                    <p style="margin-top: 0; line-height: 1.6; color: var(--text-sub);">
                        Introduce my MCP Hub system. This system allows centralized management and control of your IoT devices.
                    </p>
                    <img src="data:image/png;base64,{hub_b64}" style="width: 100%; border-radius: 8px; border: 1px solid var(--border); margin-top: 12px;">
                </div>
            </div>
        </div>
'''
    # Insert before the last closing div of .page
    replacement = '            <div class="guide-footer">\n                IotForce <span>Mcp Hub quản lý tập trung</span>Chỉ có ở <span>IotForce</span>.\n            </div>\n        </div>' + new_card + '\n    </div>'
    
    content = content.replace(target_str, replacement)
else:
    print("Warning: Target string for card insertion not found! Trying looser match.")
    # Fallback: look for just the closing divs if the footer content changed
    fallback_target = '        </div>\n    </div>\n\n    <script>'
    if fallback_target in content:
         new_card = f'''
        <div class="card" style="grid-column: 1 / -1; margin-top: 0;">
            <div class="card-header">
                <div class="brand">
                    <div class="brand-text-main">MCP Hub System</div>
                </div>
            </div>
            <div style="display: flex; gap: 24px; flex-wrap: wrap; align-items: start;">
                <div style="flex: 1; min-width: 280px;">
                    <img src="data:image/jpeg;base64,{hub2_b64}" style="width: 100%; border-radius: 8px; border: 1px solid var(--border);">
                </div>
                <div style="flex: 1; min-width: 280px;">
                    <p style="margin-top: 0; line-height: 1.6; color: var(--text-sub);">
                        Introduce my MCP Hub system.
                    </p>
                    <img src="data:image/png;base64,{hub_b64}" style="width: 100%; border-radius: 8px; border: 1px solid var(--border); margin-top: 12px;">
                </div>
            </div>
        </div>
'''
         replacement = '        </div>' + new_card + '\n    </div>\n\n    <script>'
         content = content.replace(fallback_target, replacement)
    else:
        print("Error: Could not find insertion point.")

print("Writing HTML...")
with open(html_path, 'w', encoding='utf-8') as f:
    f.write(content)

print("Done.")

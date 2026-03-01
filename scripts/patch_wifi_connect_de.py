from pathlib import Path
import re


def apply_wifi_connect_german_patch(project_root: Path | None = None) -> bool:
    root = project_root or Path(__file__).resolve().parent.parent
    html_path = root / "managed_components/78__esp-wifi-connect/assets/wifi_configuration_done.html"

    if not html_path.exists():
        print(f"[INFO] Skip German Wi-Fi page patch: {html_path} not found")
        return False

    original = html_path.read_text(encoding="utf-8")
    updated = original

    updated = re.sub(
        r"<title>.*?</title>",
        "<title>WLAN-Konfiguration</title>",
        updated,
        count=1,
        flags=re.DOTALL,
    )

    updated = re.sub(
        r"<p>配置成功！</p>\s*<p>Configuration successful!</p>",
        "<p>WLAN-Konfiguration erfolgreich!</p>\n            <p>Das Gerät verbindet sich jetzt mit deinem Netzwerk.</p>",
        updated,
        count=1,
    )

    updated = updated.replace("Configuration successful!", "WLAN-Konfiguration erfolgreich!")
    updated = updated.replace("配置成功！", "WLAN-Konfiguration erfolgreich!")

    if updated == original:
        print("[INFO] German Wi-Fi page patch already applied")
        return False

    html_path.write_text(updated, encoding="utf-8")
    print(f"[INFO] Applied German Wi-Fi page patch: {html_path}")
    return True


if __name__ == "__main__":
    apply_wifi_connect_german_patch()

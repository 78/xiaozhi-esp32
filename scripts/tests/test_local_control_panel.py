import pathlib
import re
import unittest


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
PANEL_SOURCE = PROJECT_ROOT / "main/boards/common/local_control_panel.cc"
AP_SOURCE = PROJECT_ROOT / "main/boards/kevin/box-2/secure_maintenance_ap.cc"
BOARD_SOURCE = PROJECT_ROOT / "main/boards/kevin/box-2/kevin_box_board.cc"


class LocalControlPanelContractTest(unittest.TestCase):
    def test_required_api_routes_are_registered(self):
        source = PANEL_SOURCE.read_text(encoding="utf-8")
        for route in (
            "/api/status",
            "/api/session",
            "/api/network/mode",
            "/api/settings",
            "/api/admin/password",
        ):
            self.assertIn(f'.uri = "{route}"', source)

    def test_password_storage_and_session_security_contract(self):
        source = PANEL_SOURCE.read_text(encoding="utf-8")
        self.assertIn("PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256)", source)
        iterations = re.search(r"kPbkdf2Iterations\s*=\s*([\d']+)", source)
        self.assertIsNotNone(iterations)
        self.assertGreaterEqual(int(iterations.group(1).replace("'", "")), 100_000)
        self.assertIn("HttpOnly; SameSite=Strict", source)
        self.assertIn('"X-CSRF-Token"', source)
        self.assertIn("kSessionLifetimeMs = 30 * 60 * 1000", source)
        self.assertIn("kMaxFailedLogins = 5", source)
        self.assertIn("First password must be set in maintenance mode", source)

    def test_maintenance_hotspot_is_password_protected(self):
        source = AP_SOURCE.read_text(encoding="utf-8")
        self.assertIn("WIFI_AUTH_WPA2_PSK", source)
        self.assertNotIn("WIFI_AUTH_OPEN", source)
        self.assertIn("GeneratePassword()", source)
        self.assertNotRegex(source, r"ESP_LOG\w*\([^\n]*password")

    def test_network_mode_response_precedes_transport_switch(self):
        source = PANEL_SOURCE.read_text(encoding="utf-8")
        response = source.index(
            'const esp_err_t response = SendMessage(req, "200 OK", "status", "ok");'
        )
        switch = source.index("mode_setter_(mode)", response)
        self.assertLess(response, switch)

    def test_status_payload_excludes_subscriber_and_cloud_identifiers(self):
        source = BOARD_SOURCE.read_text(encoding="utf-8").lower()
        for forbidden in ("imei", "iccid", "uuid", "cloud_credential", "mqtt_password"):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()

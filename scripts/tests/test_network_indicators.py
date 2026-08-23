import pathlib
import unittest


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
LED_SOURCE = PROJECT_ROOT / "main/led/single_led.cc"
BOARD_SOURCE = PROJECT_ROOT / "main/boards/kevin/box-2/kevin_box_board.cc"


class NetworkIndicatorContractTest(unittest.TestCase):
    def test_dual_network_idle_led_distinguishes_wifi_cellular_and_offline(self):
        source = LED_SOURCE.read_text(encoding="utf-8")
        self.assertIn("status.active == NetworkTransport::Wifi", source)
        self.assertIn("status.offline || status.active == NetworkTransport::None", source)
        self.assertIn("case kDeviceStateNetworkSwitching:", source)

    def test_kevin_oled_exposes_wake_ready_and_switch_reason(self):
        source = BOARD_SOURCE.read_text(encoding="utf-8")
        self.assertIn('"离线 · 唤醒就绪"', source)
        self.assertIn('"Wi-Fi" : "4G"', source)
        self.assertIn('"\\n原因: " + ToString(reason)', source)


if __name__ == "__main__":
    unittest.main()

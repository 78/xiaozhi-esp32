# OSTB 2ST WiFi Double Mic Power Manager

Power and display support for the OSTB 2ST WiFi Double Mic board. The board
implementation creates the manager and exposes its state through
`Board::GetBatteryLevel`.

Recovered hardware contract:

- Charge detect: GPIO47, active low
- Battery ADC: ADC2 channel 6
- Sampling: one-second timer, three-sample rolling average
- Calibration: 1985/0%, 2048/20%, 2172/40%, 2296/60%, 2420/80%, 2544/100%

The board has been validated with the listed display, audio, charging, and
battery configuration.

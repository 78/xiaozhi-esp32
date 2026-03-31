#!/bin/bash
# Generate Mosquitto password file with the two required users.
# Run: bash mosquitto/setup_passwd.sh

PASSWD_FILE="mosquitto/passwd"

# Device credentials (must match config.yaml mqtt.device_username / device_password)
mosquitto_passwd -c -b "$PASSWD_FILE" xiaozhi_device xiaozhi_device_secret

# Bridge credentials (must match config.yaml mqtt.bridge_username / bridge_password)
mosquitto_passwd -b "$PASSWD_FILE" proxy_bridge proxy_bridge_secret

echo "Password file created: $PASSWD_FILE"
echo "Users: xiaozhi_device, proxy_bridge"

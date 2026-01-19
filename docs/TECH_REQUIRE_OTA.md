# OTA Firmware Update - Technical Requirements & Implementation Guide

## 1. Overview & Workflow

This document outlines the implementation of the **Cloud-based OTA Firmware Update** feature.

### The Workflow

1.  **User Action**: The user logs into the **Web App**, selects a specific Xiaozhi device (identified by MAC address), and clicks **"Update Firmware"**.
2.  **Command Dispatch**: The Web App sends a command (via MQTT/WebSocket) to the specific Xiaozhi device.
3.  **Device Action**: The Xiaozhi device receives the command containing the **Firmware URL** (hosted on AWS S3).
4.  **Update Process**: The device downloads the firmware from AWS S3, writes it to the inactive partition, and restarts.

---

## 2. AWS S3 Firmware Hosting Setup

To allow the device to download the firmware, you must host the `.bin` file on a cloud storage service like AWS S3.

### Step 1: Create an S3 Bucket

1.  Log in to the **AWS Console** and go to **S3**.
2.  Click **Create bucket**.
3.  **Bucket Name**: Enter a unique name (e.g., `xiaozhi-firmware-updates`).
4.  **Region**: Choose a region close to your users (e.g., `ap-southeast-1` for Vietnam/Singapore).
5.  **Object Ownership**:
    - Select **ACLs enabled**.
    - Then select **Bucket owner preferred**.
    - _Why?_ This allows you to easily make individual firmware files public using the "Make public using ACL" button later.
6.  **Block Public Access settings**:
    - Uncheck **"Block all public access"**.
    - Check the warning box acknowledging that objects can be public.
    - **Security Note**: This setting allows files in this bucket to be accessible via the internet. This is necessary for the device to download the firmware without complex authentication.
      - _Risk_: Anyone with the link can download your firmware.
      - _Mitigation_: Do **not** hardcode sensitive secrets (like private API keys or passwords) in your firmware code. Use random filenames (e.g., `fw_v1_x8z9.bin`) to prevent guessing.
7.  **Bucket Versioning**: Select **Disable** (Keep it simple).
8.  **Default encryption**: Select **Server-side encryption with Amazon S3 managed keys (SSE-S3)**.
9.  Click **Create bucket**.

### Step 2: Upload Firmware

1.  Open your newly created bucket.
2.  Click **Upload**.
3.  Select your compiled firmware file (e.g., `xiaozhi-esp32.bin`).
4.  Click **Upload**.

### Step 3: Get the Public URL

1.  Click on the uploaded file name to view its properties.
2.  Go to the **Permissions** tab (or use the "Actions" menu).
3.  Select **"Make public using ACL"** (if available) or ensure the Bucket Policy allows public read.
    - _Alternative (Bucket Policy)_: Go to Bucket Permissions -> Bucket Policy and add:
      ```json
      {
        "Version": "2012-10-17",
        "Statement": [
          {
            "Sid": "PublicReadGetObject",
            "Effect": "Allow",
            "Principal": "*",
            "Action": "s3:GetObject",
            "Resource": "arn:aws:s3:::YOUR-BUCKET-NAME/*"
          }
        ]
      }
      ```
4.  Copy the **Object URL**. It should look like:
    `https://xiaozhi-firmware-updates.s3.ap-southeast-1.amazonaws.com/xiaozhi-esp32.bin`

---

## 3. Web App Implementation (Command Dispatch)

The Web App needs to send a JSON command to the device via the existing MQTT/WebSocket connection.

**Command Format (JSON):**

```json
{
  "type": "ota_url",
  "url": "https://xiaozhi-firmware-updates.s3.ap-southeast-1.amazonaws.com/xiaozhi-esp32.bin"
}
```

**Implementation Logic:**

1.  Identify the target device by **MAC Address**.
2.  Publish the JSON message above to the device's subscription topic.
    - _Topic usually follows the pattern:_ `xiaozhi/{MAC_ADDRESS}/command` (Verify with backend team).

---

## 4. Device-Side Logic (Already Implemented)

The firmware has been updated to handle the `ota_url` command.

- **Mechanism**:
  - Listens for `ota_url` JSON command.
  - Extracts the `url`.
  - Starts a background task to download and flash.
  - **Safety**: Uses A/B Partitioning. If the update fails, the old firmware remains active.
  - **Feedback**: Displays progress on the device screen ("Updating: 45%...").

---

## 5. Summary for Tech Team

| Component   | Responsibility | Action                                               |
| :---------- | :------------- | :--------------------------------------------------- |
| **AWS S3**  | Hosting        | Store `.bin` file, provide Public URL.               |
| **Web App** | Management     | User clicks "Update" -> Send JSON command to Device. |
| **Device**  | Execution      | Receive JSON -> Download from S3 -> Flash -> Reboot. |

    - An "Update Firmware" button.

2.  **Logic**:
    - Validate that the input is a valid URL.
    - Send a `POST` request to the device's IP address at `/ota_url`.
    - Show a "Update Started" message to the user.
    - Inform the user to check the device screen for progress.
    - (Optional) Poll the device status if a status API is available (currently not implemented, rely on device screen).

## Example Usage (cURL)

```bash
curl -X POST http://<DEVICE_IP>/ota_url \
     -H "Content-Type: application/json" \
     -d '{"url": "http://192.168.1.100:8000/firmware.bin"}'
```

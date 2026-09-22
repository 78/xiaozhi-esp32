# Asynchronous Voice Notifications

The `notify` message lets the cloud play a one-way voice notification while a device is idle. It does not open a conversation audio channel and never enables microphone uplink. Starting a conversation still requires an explicit wake action from the user.

## Message format

The cloud sends the following JSON through the device's current protocol control connection. For an
MQTT device, this is the existing MQTT control topic:

```json
{
  "type": "notify",
  "audio_url": "https://cdn.example.com/audio/task-complete.ogg",
  "subtitles": [
    {
      "start_ms": 0,
      "text": "Your task is complete."
    },
    {
      "start_ms": 1800,
      "text": "The result has been saved."
    }
  ]
}
```

`audio_url` is required. Both `http://` and `https://` URLs are accepted. HTTP is useful for local-network development, while production deployments can enforce HTTPS when generating the URL.

`subtitles` is optional. Each entry contains the media start time in milliseconds and the text to display. The device sorts entries by `start_ms` and updates the display only when playback crosses a new subtitle entry.

The message has no acknowledgement, notification ID, state, kind, or expiry field. Delivery is best effort and only applies to online devices.

## Audio response

`audio_url` must return a successful 2xx response containing a single-stream, mono Ogg Opus file. Responses with either `Content-Length` or chunked transfer encoding are supported. Redirects are not followed by the device.

The file is read incrementally. The device does not allocate memory based on the complete response length and does not download the complete file before playback. Notification streaming uses the existing bounded HTTP response queue, a 2 KB Ogg logical-packet buffer, the shared 20-packet Opus decode queue, and the existing two-frame PCM playback queue. The decode queue represents 1.2 seconds at the firmware's normal 60 ms packet duration. These buffers provide TCP backpressure while keeping the feature usable on devices without PSRAM.

On devices that use the standalone LiteAudioEngine WakeNet, WakeNet resources are released while a notification is playing and recreated when the device returns to `Idle`. AFE-based devices keep their existing local wake behavior.

Opus packet duration is read from the Opus TOC byte instead of being supplied in the MQTT message. Integer packet durations from 5 ms through 120 ms that are supported by the firmware decoder are accepted. The current implementation rejects stereo streams, detected Ogg or Opus structural errors, incomplete streams, oversized logical packets, and 2.5 ms packets.

## Device behavior

The device accepts `notify` only while it is in `Idle`. It then performs the following actions:

1. Enters the internal `Notifying` state and switches the board to performance mode.
2. Disables normal voice processing and microphone uplink.
3. Clears previous playback and queues the built-in popup sound.
4. Starts one HTTP GET in a background task.
5. Incrementally demultiplexes Ogg packets and sends them directly to the existing Opus decode queue.
6. Displays subtitles according to the media position of Opus packets reaching the audio output task.
7. Returns to `Idle` only after the HTTP stream has ended successfully and all queued audio has played.

The popup is queued before the HTTP task starts, so remote audio cannot play before it. HTTP connection setup still overlaps the actual popup playback.

If all playback queues drain after remote audio has started but before the HTTP stream has finished,
the device logs `Notification playback underrun #<count> at <position> ms`. The popup-to-stream
transition and normal end of playback are not reported as underruns.

The device does not open the UDP audio channel, send `start-listening`, or automatically enter `Listening`. AFE-based devices may continue local wake-word detection during playback. Devices without playback echo cancellation retain the existing speaking-mode wake behavior; hardware wake controls can still cancel a notification.

A wake action cancels the HTTP producer, clears queued notification audio, and continues through the normal wake flow. Network loss, HTTP errors, and invalid audio also cancel playback and return the device to `Idle`. A second notification received while the device is busy is ignored.

## xz-mqtt forwarding

`xz-mqtt` forwards the message with its existing Redis RPC method:

```json
{
  "method": "forward",
  "clientId": "device-client-id",
  "params": {
    "type": "notify",
    "audio_url": "https://cdn.example.com/audio/task-complete.ogg",
    "subtitles": [
      {
        "start_ms": 0,
        "text": "Your task is complete."
      }
    ]
  }
}
```

The RPC result contains the boolean returned by the MQTT send operation:

```json
{
  "success": true
}
```

This result means that `xz-mqtt` wrote the publish message to the current online device connection. It does not confirm receipt or playback. `xz-mqtt` does not create a UDP session, start the chat bridge, proxy the Ogg file, or retain notifications for offline devices.

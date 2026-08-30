import serial
import time
import sys

try:
    with serial.Serial('COM7', 115200, timeout=0.1) as s:
        s.dtr = False
        s.rts = True
        time.sleep(0.1)
        s.rts = False
        end = time.time() + 10
        while time.time() < end:
            try:
                line = s.readline()
                if line:
                    sys.stdout.buffer.write(line)
                    sys.stdout.buffer.flush()
            except Exception:
                pass
except Exception as e:
    print(f"Error: {e}")

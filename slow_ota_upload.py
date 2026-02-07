import socket
import sys
import os
import time

def send_ota_slowly(ip, filepath, chunk_size=1024, delay=0.05):
    if not os.path.isfile(filepath):
        print(f"Error: File '{filepath}' not found.")
        return

    filename = os.path.basename(filepath)
    boundary = "boundary123456789"
    port = 80
    
    # Construct the body parts manually for multipart/form-data
    
    # Header part
    body_header = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="file"; filename="{filename}"\r\n'
        "Content-Type: application/octet-stream\r\n\r\n"
    ).encode('utf-8')

    # Footer part
    body_footer = (
        f"\r\n--{boundary}--\r\n"
    ).encode('utf-8')

    file_size = os.path.getsize(filepath)
    content_length = len(body_header) + file_size + len(body_footer)

    print(f"Connecting to {ip}:{port}...")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(10)
        s.connect((ip, port))
    except Exception as e:
        print(f"Connection failed: {e}")
        return

    # Send HTTP Headers
    headers = (
        f"POST /ota_upload HTTP/1.1\r\n"
        f"Host: {ip}:{port}\r\n"
        f"Content-Type: multipart/form-data; boundary={boundary}\r\n"
        f"Content-Length: {content_length}\r\n"
        "Connection: close\r\n\r\n"
    )
    s.sendall(headers.encode('utf-8'))
    print("Headers sent.")

    # Send Body Header
    s.sendall(body_header)
    print("Body header sent. Starting file upload...")

    # Send File Content in Chunks
    sent_bytes = 0
    start_time = time.time()
    
    try:
        with open(filepath, 'rb') as f:
            while True:
                chunk = f.read(chunk_size)
                if not chunk:
                    break
                
                s.sendall(chunk)
                sent_bytes += len(chunk)
                
                # Progress update
                progress = (sent_bytes / file_size) * 100
                elapsed = time.time() - start_time
                speed = sent_bytes / elapsed if elapsed > 0 else 0
                sys.stdout.write(f"\rProgress: {progress:.2f}% - {sent_bytes}/{file_size} bytes - {speed/1024:.1f} KB/s")
                sys.stdout.flush()
                
                # Sleep to throttle upload
                if delay > 0:
                    time.sleep(delay)

        # Send Body Footer
        s.sendall(body_footer)
        print(f"\nUpload complete. Waiting for response...")

        # Read Response
        response = b""
        while True:
            try:
                chunk = s.recv(4096)
                if not chunk:
                    break
                response += chunk
                # If we see the end of HTTP response, break
                if b"\r\n0\r\n\r\n" in response or b"transfer-encoding: chunked" not in response.lower(): # Simple check
                     pass # Just read until close for now
            except socket.timeout:
                break
            except Exception as e:
                print(f"\nError reading response: {e}")
                break
                
        print("\nResponse from server:")
        print(response.decode('utf-8', errors='ignore'))

    except Exception as e:
        print(f"\nError sending data: {e}")
    finally:
        s.close()

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python slow_ota_upload.py <IP_ADDRESS> <FIRMWARE_FILE>")
        print("Example: python slow_ota_upload.py 192.168.137.74 build/xiaozhi.bin")
        sys.exit(1)

    ip_address = sys.argv[1]
    firmware_path = sys.argv[2]
    
    send_ota_slowly(ip_address, firmware_path)

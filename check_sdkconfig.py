
try:
    with open('sdkconfig', 'rb') as f:
        content = f.read()
        try:
            content.decode('cp1252')
            print("sdkconfig is valid cp1252")
        except UnicodeDecodeError as e:
            print(f"sdkconfig error: {e}")
            # print context around the error
            start = max(0, e.start - 20)
            end = min(len(content), e.end + 20)
            print(f"Context: {content[start:end]}")
except FileNotFoundError:
    print("sdkconfig not found")

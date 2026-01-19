
try:
    with open('main/Kconfig.projbuild', 'r', encoding='utf-8') as f:
        content = f.read()
        if "Đèn" in content:
            print("Found 'Đèn' in main/Kconfig.projbuild")
        else:
            print("Not found 'Đèn' in main/Kconfig.projbuild")
except Exception as e:
    print(f"Error: {e}")

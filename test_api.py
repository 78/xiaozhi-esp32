import requests
import json

url = "http://192.168.3.118:8081/app-api/xiaoqiao/system/skin"
headers = {
    "Device-Id": "b4:3a:45:a5:90:14",
    "Client-Id": "9c34d553-bd79-453f-a4fa-6bcdabe8156d"
}

try:
    response = requests.get(url, headers=headers)
    print(f"状态码: {response.status_code}")
    print(f"响应头: {response.headers}")
    print(f"响应内容: {response.text}")
    
    if response.status_code == 200:
        try:
            json_data = response.json()
            print(f"JSON格式化输出:")
            print(json.dumps(json_data, indent=2, ensure_ascii=False))
        except json.JSONDecodeError:
            print("响应不是有效的JSON格式")
    
except Exception as e:
    print(f"请求失败: {e}")
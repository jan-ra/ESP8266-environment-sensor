from Crypto.Cipher import AES
from Crypto.Util.Padding import pad
import base64
import config

b = bytearray()
b.extend(map(ord, config.keystr))
key = b

with open('data.txt', 'r') as f:
    lines = f.readlines()

with open('out.csv', 'w') as f:
    for line in lines:
        cipher = AES.new(key, AES.MODE_CBC, config.iv)
        base64_str = line.strip()
        ct_bytes = base64.b64decode(base64_str)
        pt_bytes = cipher.decrypt(ct_bytes)
        pt = pt_bytes.decode().rstrip('X')
        f.write(f"{pt}\n")

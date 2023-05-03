from Crypto.Cipher import AES
from Crypto.Util.Padding import pad
import base64
import config

b = bytearray()
b.extend(map(ord, config.keystr))
key = b
cipher = AES.new(key, AES.MODE_CBC, config.iv)

base64_str = "PlO2DwRAoeM/XCu0Yr4ODknDbWo5qMwVKZ6rSzF9mwNgorlTgEo7jf++U0xAtCgKgmpDFDyy2mGWZfUes53fNg=="
ct_bytes = base64.b64decode(base64_str)
pt_bytes = cipher.decrypt(ct_bytes)
pt = pt_bytes.decode()
print(f"Decoded text: {pt}")

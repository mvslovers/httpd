# usage: xfer.py get TARGET DSN FILE | put TARGET DSN FILE VOLUME
# Binary download / upload (new FB80 data set) of an XMIT through mvsMF.
import tomllib, os, subprocess, sys, json
op, t, dsn, fn = sys.argv[1:5]
m = tomllib.load(open(t[1:], "rb")) if t.startswith("@") else tomllib.load(open(os.path.expanduser("~/.mbt/targets.toml"), "rb"))["target"][t]["mvsmf"]
pw = m["password"]
if isinstance(pw, dict):
    pw = subprocess.run(["security", "find-generic-password", "-s", pw["keychain"], "-w"], capture_output=True, text=True).stdout.strip()
cfg = f'user = "{m["user"]}:{pw}"\n'; base = m["url"].rstrip("/")
def curl(*a):
    r = subprocess.run(["curl", "-s", "-m", "3600", "-w", "\n%{http_code}", "-K", "-"] + list(a), input=cfg.encode(), capture_output=True)
    body, _, code = r.stdout.rpartition(b"\n"); return code.decode(), body
if op == "get":
    code, body = curl("-H", "X-IBM-Data-Type: binary", f"{base}/zosmf/restfiles/ds/{dsn}")
    print("GET", code, len(body)); open(fn, "wb").write(body)
else:
    vol = sys.argv[5]
    curl("-X", "DELETE", f"{base}/zosmf/restfiles/ds/{dsn}")
    attrs = {"dsorg": "PS", "alcunit": "TRK", "primary": os.path.getsize(fn) // 40000 + 20, "secondary": 50, "recfm": "FB",
             "lrecl": 80, "blksize": 3120, "unit": "SYSDA", "vol": vol}
    code, body = curl("-X", "POST", "-H", "Content-Type: application/json", "-d", json.dumps(attrs), f"{base}/zosmf/restfiles/ds/{dsn}")
    print("CREATE", code, body[:200])
    code, body = curl("-X", "PUT", "-H", "Content-Type: application/octet-stream", "-H", "X-IBM-Data-Type: binary",
                      "-H", "Expect:", "--data-binary", "@" + fn, f"{base}/zosmf/restfiles/ds/{dsn}")
    print("PUT", code, body[:200])

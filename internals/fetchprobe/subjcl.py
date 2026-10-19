# usage: subjcl.py TARGET FILE.jcl -- submit, wait, print all spool records
import tomllib, os, subprocess, sys, json, time
t, fn = sys.argv[1], sys.argv[2]
m = tomllib.load(open(t[1:], "rb")) if t.startswith("@") else tomllib.load(open(os.path.expanduser("~/.mbt/targets.toml"), "rb"))["target"][t]["mvsmf"]
pw = m["password"]
if isinstance(pw, dict):
    pw = subprocess.run(["security", "find-generic-password", "-s", pw["keychain"], "-w"], capture_output=True, text=True).stdout.strip()
cfg = f'user = "{m["user"]}:{pw}"\n'; base = m["url"].rstrip("/")
def curl(*a):
    return subprocess.run(["curl", "-s", "-m", "120", "-K", "-"] + list(a), input=cfg, capture_output=True, text=True, errors="replace").stdout
r = json.loads(curl("-X", "PUT", "-H", "Content-Type: text/plain", "-H", "Expect:", "--data-binary", "@" + fn, base + "/zosmf/restjobs/jobs"))
if "jobid" not in r: print("rejected", r); sys.exit(1)
jn, jid = r["jobname"], r["jobid"]; print("submitted", jn, jid, flush=True)
while json.loads(curl(f"{base}/zosmf/restjobs/jobs/{jn}/{jid}")).get("status") != "OUTPUT":
    time.sleep(10)
for f in json.loads(curl(f"{base}/zosmf/restjobs/jobs/{jn}/{jid}/files")):
    print(f"===== {f['ddname']} {f.get('stepname')}")
    print(curl(f"{base}/zosmf/restjobs/jobs/{jn}/{jid}/files/{f['id']}/records"))

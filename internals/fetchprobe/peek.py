# usage: peek.py TARGET JOBNAME JOBID -- status + LDLP lines of JESMSGLG, also while the job runs
import tomllib, os, subprocess, sys, json, re
t, jn, jid = sys.argv[1:4]
m = tomllib.load(open(t[1:], "rb")) if t.startswith("@") else tomllib.load(open(os.path.expanduser("~/.mbt/targets.toml"), "rb"))["target"][t]["mvsmf"]
pw = m["password"]
if isinstance(pw, dict):
    pw = subprocess.run(["security", "find-generic-password", "-s", pw["keychain"], "-w"], capture_output=True, text=True).stdout.strip()
cfg = f'user = "{m["user"]}:{pw}"\n'; base = m["url"].rstrip("/")
def curl(*a):
    return subprocess.run(["curl", "-s", "-m", "120", "-K", "-"] + list(a), input=cfg, capture_output=True, text=True, errors="replace").stdout
s = json.loads(curl(f"{base}/zosmf/restjobs/jobs/{jn}/{jid}"))
print("status", s.get("status"), "retcode", s.get("retcode"))
for f in json.loads(curl(f"{base}/zosmf/restjobs/jobs/{jn}/{jid}/files")):
    if f["ddname"] == "JESMSGLG":
        for l in curl(f"{base}/zosmf/restjobs/jobs/{jn}/{jid}/files/{f['id']}/records").splitlines():
            if re.search(r"LDLP|IEA703I|STARTED", l): print(l.rstrip())

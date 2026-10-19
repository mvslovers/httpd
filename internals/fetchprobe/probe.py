# usage: probe.py TARGET NAME:COUNT[:DSN] ...
# Builds the fetch probe job from ldlp.jcl and ldlp.asm (assemble, link, one
# GO step per NAME:COUNT, DSN added to that step's STEPLIB after the probe's
# own library), submits it through mvsMF, waits, and prints the step RCs and
# every LDLP line from the job log.  DRYRUN=1 prints the JCL instead.
# TARGET is a [target.NAME] of ~/.mbt/targets.toml, or @FILE naming a TOML
# file with url, user and password; credentials go to curl
# through -K -, never on the command line.
import tomllib, os, subprocess, sys, json, time, re
t, specs = sys.argv[1], sys.argv[2:]
here = os.path.dirname(os.path.abspath(__file__))
m = tomllib.load(open(t[1:], "rb")) if t.startswith("@") else tomllib.load(open(os.path.expanduser("~/.mbt/targets.toml"), "rb"))["target"][t]["mvsmf"]
pw = m["password"]
if isinstance(pw, dict):
    pw = subprocess.run(["security", "find-generic-password", "-s", pw["keychain"], "-w"],
                        capture_output=True, text=True).stdout.strip()
cfg = f'user = "{m["user"]}:{pw}"\n'
base = m["url"].rstrip("/")

def curl(*a, data=None):
    args = ["curl", "-s", "-m", "120", "-K", "-"] + list(a)
    if data is not None:
        fn = os.path.join(here, f".body-{os.getpid()}.jcl")
        open(fn, "w").write(data)
        args += ["--data-binary", "@" + fn]
    try:
        return subprocess.run(args, input=cfg, capture_output=True, text=True, errors="replace").stdout
    finally:
        if data is not None:
            os.remove(fn)

jcl = open(os.path.join(here, "ldlp.jcl")).read()
for i, sp in enumerate(specs):
    f = sp.split(":")
    jcl += f"//GO{i}      EXEC PGM=LDLP,PARM='{f[0]},{f[1]}',COND=(0,NE,LKED)\n"
    jcl += "//STEPLIB  DD DSN=&&LOAD,DISP=(OLD,PASS)\n"
    if len(f) > 2:
        jcl += f"//         DD DSN={f[2]},DISP=SHR\n"
jcl += "//\n"
src = open(os.path.join(here, "ldlp.asm")).read().rstrip("\n")
a = jcl.index("//SYSIN    DD *\n") + len("//SYSIN    DD *\n")
jcl = jcl[:a] + src + "\n" + jcl[a:]
assert all(len(l) <= 80 for l in jcl.split("\n"))
if os.environ.get("DRYRUN"):
    print(jcl); sys.exit(0)
r = json.loads(curl("-X", "PUT", "-H", "Content-Type: text/plain", "-H", "Expect:",
                    base + "/zosmf/restjobs/jobs", data=jcl))
if "jobid" not in r: print("submit rejected:", r); sys.exit(1)
jn, jid = r["jobname"], r["jobid"]
print("submitted", jn, jid, flush=True)
t0 = time.time()
while True:
    s = json.loads(curl(f"{base}/zosmf/restjobs/jobs/{jn}/{jid}"))
    if s.get("status") == "OUTPUT":
        break
    time.sleep(15)
print("status", s.get("status"), "retcode", s.get("retcode"), f"elapsed ~{time.time()-t0:.0f}s")
files = json.loads(curl(f"{base}/zosmf/restjobs/jobs/{jn}/{jid}/files"))
for f in files:
    if f["ddname"] in ("JESMSGLG", "JESYSMSG"):
        txt = curl(f"{base}/zosmf/restjobs/jobs/{jn}/{jid}/files/{f['id']}/records")
        for l in txt.splitlines():
            if re.search(r"LDLP|IEF142I|IEF272I|ABEND|IEA703I|IEW|RC=|COND CODE", l):
                print(f["ddname"][:3], l.rstrip())

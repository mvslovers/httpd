#!/usr/bin/env python3
"""Soak driver: repeated mvsMF/display-module requests with validation, storage
samples between phases. Output: JSON lines in --out (samples, summary), one JSON
line per HTTP attempt in --req-out (default OUT.req: kind, worker, keep-alive,
connection, connect/send/first-byte/body times) and host-pressure probes in the
same file; summary on stdout. Analyse with tests/soak-stalls.py (issue #350)."""
import argparse, base64, http.client, itertools, json, os, random, re, sys, threading, time, urllib.parse
from collections import Counter

ap = argparse.ArgumentParser()
ap.add_argument("--url", required=True)
ap.add_argument("--user", required=True)
ap.add_argument("--password", required=True)
ap.add_argument("--version", required=True, help="expected mvsMF version string")
ap.add_argument("--out", required=True)
ap.add_argument("--phases", type=int, default=3)
ap.add_argument("--phase-min", type=float, default=11.0, help="minimum minutes per phase")
ap.add_argument("--phase-reqs", type=int, default=1700, help="minimum requests per phase")
ap.add_argument("--slice-sec", type=int, default=150)
ap.add_argument("--levels", default="1,3,6,9")
ap.add_argument("--label", default="run")
ap.add_argument("--seed", type=int, default=461)
ap.add_argument("--skip", default="", help="comma list of request kinds left out (diagnostic runs)")
ap.add_argument("--no-display", action="store_true", help="display modules not routed")
ap.add_argument("--req-out", default=None, help="per-attempt records (default: OUT.req)")
ap.add_argument("--probe-sec", type=float, default=5.0, help="host probe period, 0 = off")
ap.add_argument("--cgroup", default=None, help="cgroup dir of the container under test (cpu.pressure, cpu.stat)")
ap.add_argument("--timeout", type=float, default=60.0, help="client timeout per attempt")
a = ap.parse_args()

u = urllib.parse.urlparse(a.url)
AUTH = "Basic " + base64.b64encode(f"{a.user}:{a.password}".encode()).decode()
HDR = {"Authorization": AUTH, "X-CSRF-ZOSMF-HEADER": "x"}
out = open(a.out, "a", buffering=1)
olock = threading.Lock()
reqf = open(a.req_out or a.out + ".req", "a", buffering=1)
rlock = threading.Lock()
SEQ = itertools.count(1)
CONN_IDS = itertools.count(1)


def emit_req(rec):
    with rlock:
        reqf.write(json.dumps(rec, separators=(",", ":")) + "\n")


class Conn:
    def __init__(self):
        self.c = None
        self.id = 0
        self.n = 0

    def drop(self):
        try:
            if self.c:
                self.c.close()
        except Exception:
            pass
        self.c = None


def emit(kind, **kw):
    kw.update(kind=kind, t=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()))
    with olock:
        out.write(json.dumps(kw) + "\n")


def has(*needles):
    return lambda body: all(n in body for n in needles)


def jobs_ok(body):
    j = json.loads(body)
    return isinstance(j, list) and (not j or "jobname" in j[0])


def ds_ok(body):
    j = json.loads(body)
    return j.get("returnedRows") == 1 and j["items"][0]["dsname"] == "SYS2.PROCLIB"


# name, weight, path, validator
KINDS = [
    ("version", 30, "/zosmf/test?fn=version", has(f'"version": "{a.version}"')),
    ("info", 15, "/zosmf/info", has('"zosmf_full_version":"' + a.version + '"')),
    ("dslist", 15, "/zosmf/restfiles/ds?dslevel=SYS2.PROCLIB", ds_ok),
    ("member", 15, "/zosmf/restfiles/ds/SYS2.PROCLIB(HTTPD)", lambda b: b.startswith("//HTTPD")),
    ("memlist", 10, "/zosmf/restfiles/ds/SYS2.PROCLIB/member", has('"member": "HTTPD"')),
    ("jobs", 10, "/zosmf/restjobs/jobs?owner=*&prefix=*", jobs_ok),
]
if not a.no_display:
    KINDS += [
        ("dsrv", 3, "/.dsrv?target=HTTPD", has("resident_held")),
        ("dm", 2, "/.dm?m=10&l=16", has("00000000")),
    ]
KINDS = [k for k in KINDS if k[0] not in a.skip.split(",")]
WEIGHTS = [k[1] for k in KINDS]
BYNAME = {k[0]: k for k in KINDS}

stats_lock = threading.Lock()
S = Counter()
PERKIND = {k[0]: Counter() for k in KINDS}
LAT = []
FAILS = []


def one(h, kind, keepalive, ctx):
    name, _, path, check = BYNAME[kind]
    t0 = time.perf_counter()
    status = body = None
    err = None
    for attempt in (0, 1):
        rec = dict(seq=next(SEQ), t=round(time.time(), 3), kind=kind, attempt=attempt,
                   ka=keepalive, **ctx)
        p0 = time.perf_counter()
        try:
            if h.c is None:
                h.c = http.client.HTTPConnection(u.hostname, u.port, timeout=a.timeout)
                h.id = next(CONN_IDS)
                h.n = 0
                tc = time.perf_counter()
                h.c.connect()
                rec["t_conn"] = round(time.perf_counter() - tc, 4)
            rec["conn"] = h.id
            rec["conn_n"] = h.n
            h.n += 1
            hdrs = dict(HDR)
            if not keepalive:
                hdrs["Connection"] = "close"
            ts = time.perf_counter()
            h.c.request("GET", path, headers=hdrs)
            tr = time.perf_counter()
            rec["t_send"] = round(tr - ts, 4)
            r = h.c.getresponse()
            tb = time.perf_counter()
            rec["t_ttfb"] = round(tb - tr, 4)
            body = r.read().decode("latin-1")
            rec["t_body"] = round(time.perf_counter() - tb, 4)
            status = r.status
            rec["status"] = status
            rec["bytes"] = len(body)
            rec["rconn"] = r.getheader("Connection", "").lower()
            if rec["rconn"] == "close" or not keepalive:
                h.drop()
            rec["dt"] = round(time.perf_counter() - p0, 4)
            emit_req(rec)
            break
        except Exception as e:  # transport error
            err = f"{type(e).__name__}: {e}"
            rec["err"] = type(e).__name__
            rec["dt"] = round(time.perf_counter() - p0, 4)
            emit_req(rec)
            h.drop()
            with stats_lock:
                S["transport_errors"] += 1
                PERKIND[kind]["transport_errors"] += 1
                if len(FAILS) < 50:
                    FAILS.append(("transport", kind, err, time.strftime("%T")))
            if attempt == 1:
                status = None
            else:
                with stats_lock:
                    S["retried"] += 1
    dt = time.perf_counter() - t0
    ok = False
    reason = None
    if status is None:
        reason = "no response: " + str(err)
    elif status != 200:
        reason = f"HTTP {status}"
    else:
        try:
            ok = bool(check(body))
            if not ok:
                reason = "unexpected body: " + body[:120].replace("\n", " ")
        except Exception as e:
            reason = f"validator {type(e).__name__}: {e}; body: " + body[:80].replace("\n", " ")
    with stats_lock:
        S["requests"] += 1
        PERKIND[kind]["requests"] += 1
        if ok:
            S["ok"] += 1
            PERKIND[kind]["ok"] += 1
        else:
            S["failed"] += 1
            PERKIND[kind]["failed"] += 1
            if len(FAILS) < 50:
                FAILS.append(("fail", kind, reason, time.strftime("%T")))
        LAT.append(dt)
        PERKIND[kind].setdefault("lat", []).append(dt)
    return ok


def run_slice(level, seconds, rng_seed, phase, slice_no):
    stop = time.time() + seconds
    threads = []

    def worker(i):
        rng = random.Random(rng_seed * 100 + i)
        holder = Conn()
        keep = (i % 2 == 0)  # half the workers keep the connection alive
        ctx = dict(worker=i, level=level, phase=phase, slice=slice_no)
        while time.time() < stop:
            one(holder, rng.choices([k[0] for k in KINDS], WEIGHTS)[0], keep, ctx)
        holder.drop()

    for i in range(level):
        t = threading.Thread(target=worker, args=(i,))
        t.start()
        threads.append(t)
    for t in threads:
        t.join()


def sample(label, warm):
    # the same fixed burst before every sample: n requests of each kind, serial
    if warm:
        holder = Conn()
        for k in KINDS:
            for _ in range(5):
                one(holder, k[0], False, dict(worker=-1, level=0, phase=0, slice=0))
        time.sleep(3)
    rec = {"label": label, "warm": warm}
    c = http.client.HTTPConnection(u.hostname, u.port, timeout=120)
    for key, path in (("storage", "/zosmf/test?fn=storage&total=1"),
                      ("storage_largest", "/zosmf/test?fn=storage")):
        c.request("GET", path, headers=dict(HDR, Connection="close"))
        r = c.getresponse()
        body = r.read().decode("latin-1")
        c.close()
        try:
            rec[key] = json.loads(body)
        except Exception:
            rec[key] = {"raw": body[:300], "status": r.status}
    if not a.no_display:
        for key, path in (("dsrv_httpd", "/.dsrv?target=HTTPD"), ("dsrv_mgr", "/.dsrv?target=MGR")):
            c.request("GET", path, headers=dict(HDR, Connection="close"))
            r = c.getresponse()
            body = r.read().decode("latin-1")
            c.close()
            rec[key + "_status"] = r.status
            text = re.sub(r"<[^>]+>", " ", body)
            rec[key] = re.sub(r"\s+", " ", text)[:6000]
    with stats_lock:
        rec["cumulative"] = dict(S)
    emit("sample", **rec)
    st = rec["storage"]
    print(f"[{time.strftime('%T')}] sample {label}: largest={st.get('largest')} total={st.get('total')} "
          f"blocks={st.get('blocks')} free_errors={st.get('free_errors')} requests={S['requests']} "
          f"failed={S['failed']} transport={S['transport_errors']}", flush=True)
    return rec


def pct(xs):
    xs = sorted(xs)
    if not xs:
        return {}
    return {"p50": xs[len(xs) // 2], "p95": xs[int(len(xs) * .95)], "max": xs[-1]}


def read1(path):
    try:
        with open(path) as f:
            return f.read()
    except OSError:
        return ""


def psi_total(text):
    m = re.search(r"some .*total=(\d+)", text)
    return int(m.group(1)) if m else None


def probe(stop):
    # Host side of the story: 1 s sleep overshoot of this process, load, CPU pressure
    # (time some runnable task waited for a CPU) of the host and of the container.
    if a.probe_sec <= 0:
        return
    last = last_emit = time.time()
    lag_max = 0.0
    while not stop.wait(1.0):
        now = time.time()
        lag_max = max(lag_max, now - last - 1.0)
        last = now
        if now - last_emit < a.probe_sec:
            continue
        last_emit = now
        rec = dict(kind="probe", t=round(time.time(), 3), lag_max=round(lag_max, 3))
        lag_max = 0.0
        rec["load1"] = read1("/proc/loadavg").split(" ")[0]
        rec["psi_host_us"] = psi_total(read1("/proc/pressure/cpu"))
        if a.cgroup:
            rec["psi_cg_us"] = psi_total(read1(os.path.join(a.cgroup, "cpu.pressure")))
            m = re.search(r"usage_usec (\d+)", read1(os.path.join(a.cgroup, "cpu.stat")))
            rec["cg_usage_us"] = int(m.group(1)) if m else None
        emit_req(rec)


def main():
    pstop = threading.Event()
    pt = threading.Thread(target=probe, args=(pstop,), daemon=True)
    pt.start()
    levels = [int(x) for x in a.levels.split(",")]
    emit("start", args=vars(a) | {"password": "***"})
    sample("start0", warm=False)
    sample("base", warm=True)
    for ph in range(1, a.phases + 1):
        t0 = time.time()
        r0 = S["requests"]
        k = 0
        while True:
            lvl = levels[(k + ph - 1) % len(levels)]
            run_slice(lvl, a.slice_sec, a.seed + ph * 1000 + k, ph, k + 1)
            k += 1
            el = time.time() - t0
            n = S["requests"] - r0
            print(f"[{time.strftime('%T')}] phase {ph} slice {k} level {lvl}: phase_requests={n} "
                  f"elapsed={el/60:.1f}min total={S['requests']} failed={S['failed']} "
                  f"transport={S['transport_errors']}", flush=True)
            if el >= a.phase_min * 60 and n >= a.phase_reqs and k >= len(levels):
                break
        emit("phase_end", phase=ph, requests=S["requests"] - r0, minutes=(time.time() - t0) / 60)
        sample(f"after_phase{ph}", warm=True)
    pstop.set()
    lat = sorted(LAT)
    summ = {"requests": S["requests"], "ok": S["ok"], "failed": S["failed"],
            "transport_errors": S["transport_errors"], "retried": S["retried"],
            "latency_p50": lat[len(lat) // 2] if lat else None,
            "latency_p95": lat[int(len(lat) * .95)] if lat else None,
            "latency_max": lat[-1] if lat else None,
            "per_kind": {k: {x: y for x, y in v.items() if x != "lat"} | pct(v.get("lat", []))
                         for k, v in PERKIND.items()},
            "first_failures": FAILS[:50]}
    emit("summary", **summ)
    print(json.dumps(summ, indent=1))
    sys.exit(0 if S["failed"] == 0 else 1)


main()

#!/usr/bin/env python3
"""Analyse the per-attempt records of tests/soak.py (issue #350): latency per
request kind, every attempt slower than a threshold with its context, and whether
the slow attempts cluster on a request kind, keep-alive vs close, a fresh vs a
reused connection, a client worker or a load level. Output is Markdown.

usage: soak-stalls.py RUN.jsonl.req [--threshold 5] [--all-attempts]"""
import argparse, bisect, json, sys
from collections import Counter, defaultdict
from datetime import datetime, timezone

ap = argparse.ArgumentParser()
ap.add_argument("req")
ap.add_argument("--threshold", type=float, default=5.0, help="seconds; an attempt at or above is a stall")
a = ap.parse_args()

att, probes = [], []
for line in open(a.req):
    line = line.strip()
    if not line:
        continue
    r = json.loads(line)
    (probes if r.get("kind") == "probe" else att).append(r)
att = [r for r in att if r.get("worker", -1) >= 0]  # drop the storage-sample bursts
probes.sort(key=lambda r: r["t"])
if not att:
    sys.exit("no attempts in " + a.req)


def q(xs, p):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(len(xs) * p))]


def hms(t):
    return datetime.fromtimestamp(t, timezone.utc).strftime("%H:%M:%S")


ends = sorted(r["t"] + r["dt"] for r in att)
stalls = sorted((r for r in att if r["dt"] >= a.threshold), key=lambda r: r["t"])
t0 = min(r["t"] for r in att)
t1 = max(r["t"] + r["dt"] for r in att)
errs = [r for r in att if "err" in r]

print(f"# Soak latency analysis\n")
print(f"{len(att)} attempts in {(t1 - t0) / 60:.1f} min ({sum(1 for r in att if r['attempt'] == 0)} requests, "
      f"{sum(1 for r in att if r['attempt'] == 1)} retries), {len(errs)} transport errors, "
      f"stall threshold {a.threshold:g} s: **{len(stalls)} attempts**.\n")

print("## Latency per request kind (attempt level, seconds)\n")
print("| kind | n | p50 | p95 | p99 | max | >=1 s | >=threshold |")
print("|---|--:|--:|--:|--:|--:|--:|--:|")
bykind = defaultdict(list)
for r in att:
    bykind[r["kind"]].append(r["dt"])
for k, xs in sorted(bykind.items()):
    print(f"| {k} | {len(xs)} | {q(xs, .5):.3f} | {q(xs, .95):.3f} | {q(xs, .99):.3f} | {max(xs):.3f} | "
          f"{sum(x >= 1 for x in xs)} | {sum(x >= a.threshold for x in xs)} |")
allx = [r["dt"] for r in att]
print(f"| **all** | {len(allx)} | {q(allx, .5):.3f} | {q(allx, .95):.3f} | {q(allx, .99):.3f} | {max(allx):.3f} | "
      f"{sum(x >= 1 for x in allx)} | {len(stalls)} |\n")

print("## Where the time goes in the slow attempts\n")
print("Phases of one attempt: connect, send, first byte (server think time), body. "
      "A stall in `connect` is the TCP accept path; in `ttfb` it is the server or the module.\n")
print("| bucket | n | connect | send | ttfb | body | transport error |")
print("|---|--:|--:|--:|--:|--:|--:|")
for lo, hi in ((1, 5), (5, 10), (10, 30), (30, 1e9)):
    rows = [r for r in att if lo <= r["dt"] < hi]
    if not rows:
        continue
    def dom(r):
        if "err" in r:
            return "err"
        parts = {"connect": r.get("t_conn", 0), "send": r.get("t_send", 0),
                 "ttfb": r.get("t_ttfb", 0), "body": r.get("t_body", 0)}
        return max(parts, key=parts.get)
    c = Counter(dom(r) for r in rows)
    lab = f"{lo:g}-{hi:g} s" if hi < 1e9 else f"{lo:g}+ s"
    print(f"| {lab} | {len(rows)} | {c['connect']} | {c['send']} | {c['ttfb']} | {c['body']} | {c['err']} |")
print("\n(each count is the number of attempts whose longest phase is that column)\n")


def window_psi(key, ta, tb):
    ps = [p for p in probes if p.get(key) is not None]
    if len(ps) < 2:
        return None
    i = bisect.bisect_left([p["t"] for p in ps], ta)
    j = bisect.bisect_right([p["t"] for p in ps], tb) - 1
    i = min(i, len(ps) - 2)
    j = max(j, i + 1)
    dt = ps[j]["t"] - ps[i]["t"]
    return None if dt <= 0 else 100.0 * (ps[j][key] - ps[i][key]) / 1e6 / dt


def window_lag(ta, tb):
    xs = [p["lag_max"] for p in probes if ta <= p["t"] <= tb + 5]
    return max(xs) if xs else None


def window_load(ta, tb):
    xs = [float(p["load1"]) for p in probes if ta <= p["t"] <= tb + 5]
    return max(xs) if xs else None


# completions per second by the whole generator in the same slice, for the "others" comparison
slice_rate = {}
byslice = defaultdict(list)
for r in att:
    byslice[(r["phase"], r["slice"])].append(r)
for k, rows in byslice.items():
    span = max(r["t"] + r["dt"] for r in rows) - min(r["t"] for r in rows)
    slice_rate[k] = len(rows) / span if span > 0 else 0

if stalls:
    print(f"## Every attempt >= {a.threshold:g} s\n")
    print("`others done` = attempts of other workers that finished while this one was stuck; "
          "`expected` = what the slice's average rate predicts for that window. "
          "Host/ctr CPU = share of the window in which a runnable task waited for a CPU "
          "(pressure stall, host-wide / container cgroup). `lag` = worst client sleep overshoot.\n")
    print("| time (UTC) | kind | worker | ka | conn#req | lvl | dt | conn | send | ttfb | body | err | others done / expected | host CPU wait | ctr CPU wait | lag | load |")
    print("|---|---|--:|---|--:|--:|--:|--:|--:|--:|--:|---|--:|--:|--:|--:|--:|")
    for r in stalls:
        ta, tb = r["t"], r["t"] + r["dt"]
        done = bisect.bisect_right(ends, tb) - bisect.bisect_left(ends, ta) - 1
        exp = slice_rate.get((r["phase"], r["slice"]), 0) * r["dt"]
        hp, cp = window_psi("psi_host_us", ta, tb), window_psi("psi_cg_us", ta, tb)
        lag, load = window_lag(ta, tb), window_load(ta, tb)
        f = lambda v, fmt="{:.0f}%": "-" if v is None else fmt.format(v)
        print(f"| {hms(ta)} | {r['kind']} | {r['worker']} | {'ka' if r['ka'] else 'close'} | "
              f"{r.get('conn', '-')}#{r.get('conn_n', '-')} | {r['level']} | {r['dt']:.1f} | "
              f"{r.get('t_conn', 0):.1f} | {r.get('t_send', 0):.1f} | {r.get('t_ttfb', 0):.1f} | "
              f"{r.get('t_body', 0):.1f} | {r.get('err', '')} | {done} / {exp:.0f} | {f(hp)} | {f(cp)} | "
              f"{f(lag, '{:.1f}s')} | {f(load, '{:.1f}')} |")
    print()

    print("## Correlation: share of the stalls vs share of all attempts\n")
    print("A factor correlates only if the stall column clearly departs from the all column "
          "and the counts are large enough to mean something (a handful of stalls proves nothing).\n")

    def factor(name, fn):
        tot, st = Counter(fn(r) for r in att), Counter(fn(r) for r in stalls)
        print(f"**{name}**\n")
        print("| value | attempts | share | stalls | share | stall rate |")
        print("|---|--:|--:|--:|--:|--:|")
        for v in sorted(tot, key=str):
            print(f"| {v} | {tot[v]} | {100 * tot[v] / len(att):.0f}% | {st[v]} | "
                  f"{100 * st[v] / len(stalls):.0f}% | {100 * st[v] / tot[v]:.2f}% |")
        print()

    factor("request kind", lambda r: r["kind"])
    factor("keep-alive worker vs close worker", lambda r: "keep-alive" if r["ka"] else "close")
    factor("connection: fresh (first request) vs reused", lambda r: "fresh" if r.get("conn_n", 0) == 0 else "reused")
    factor("client load level (workers)", lambda r: r["level"])
    factor("client worker", lambda r: r["worker"])
    factor("attempt", lambda r: "retry" if r["attempt"] else "first")

    ov = 0
    for r in stalls:
        if any(o is not r and o["t"] < r["t"] + r["dt"] and r["t"] < o["t"] + o["dt"] for o in stalls):
            ov += 1
    print(f"Stalls that overlap in time with another stall: {ov} of {len(stalls)}.\n")
else:
    print(f"No attempt reached {a.threshold:g} s.\n")

if probes:
    ps = [float(p["load1"]) for p in probes if p.get("load1")]
    print("## Host during the run\n")
    print(f"- load average (1 min) seen by the probe: min {min(ps):.1f}, median {q(ps, .5):.1f}, max {max(ps):.1f}")
    hp = window_psi("psi_host_us", t0, t1)
    cp = window_psi("psi_cg_us", t0, t1)
    print(f"- host CPU pressure over the whole run: {'-' if hp is None else f'{hp:.1f}%'} of the time some task waited for a CPU")
    print(f"- container CPU pressure over the whole run: {'-' if cp is None else f'{cp:.1f}%'}")
    print(f"- worst client sleep overshoot: {max(p['lag_max'] for p in probes):.2f} s")

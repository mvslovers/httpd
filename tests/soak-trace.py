#!/usr/bin/env python3
"""Join the stalls seen by the client (tests/soak.py, OUT.req) to the records of the
server-side request trace (OUT.trace, from --trace-sec; server Parmlib TRACE=n) and say, per
stall, which worker served it and in which phase the time went (issue #350). Output is Markdown.

usage: soak-trace.py RUN.jsonl [--threshold 5]
reads RUN.jsonl.req and RUN.jsonl.trace

A stall is matched to the record with the same client port and request number on the
connection whose life overlaps the stall; the record's start is placed on this host's clock as
(arrival of the reply - 'ago'), so the server clock plays no part. The phase of a record is the
largest gap between two consecutive marks, the last mark to the readout counting for a request
that was still running."""
import argparse, json, sys
from collections import Counter, defaultdict

ap = argparse.ArgumentParser()
ap.add_argument("run")
ap.add_argument("--threshold", type=float, default=5.0, help="seconds; an attempt at or above is a stall")
ap.add_argument("--slack", type=float, default=2.0, help="seconds of tolerance when placing a record on the clock")
a = ap.parse_args()

MARKS = ["run", "line1", "req", "parse", "cred", "route", "lnk0", "lnk1", "snd1", "sndl", "done", "end", "close"]
INT = {"seq", "port", "n", "resp", "ago", "live", "age", "polls", "txn", "txst", "txms", "sent"}


def parse(line):
    d = {}
    for w in line.split(" "):
        k, _, v = w.partition("=")
        if k in INT or k in MARKS:
            d[k] = None if v == "-" else int(v)
        else:
            d[k] = v
    return d


att = []
for line in open(a.run + ".req"):
    line = line.strip()
    if line:
        r = json.loads(line)
        if r.get("kind") != "probe" and r.get("worker", -1) >= 0:
            att.append(r)

recs = {}        # seq -> latest observation, with wall_open
polls = full = 0
slots = lost = miss = workers = None
for line in open(a.run + ".trace"):
    p = json.loads(line)
    if "lines" not in p or p.get("status") != 200:
        continue
    polls += 1
    full += p["min"] == 0
    for l in p["lines"]:
        if l.startswith("#HTTPTRC v1"):
            h = dict(w.split("=", 1) for w in l.split(" ")[2:] if "=" in w)
            slots, lost, miss, workers = h.get("slots"), h.get("lost"), h.get("miss"), h.get("workers")
        elif l.startswith("seq="):
            d = parse(l)
            d["wall"] = p["t_recv"] - d["ago"] / 1000.0
            d["full"] = p["min"] == 0
            old = recs.get(d["seq"])
            if old is None or d["ago"] >= old["ago"]:
                d["full"] = d["full"] or (old or {}).get("full", False)
                recs[d["seq"]] = d
if not recs:
    sys.exit("no trace records in " + a.run + ".trace")


def gaps(d):
    pts = [("q", 0)] + [(m, d[m]) for m in MARKS if d.get(m) is not None]
    pts.sort(key=lambda x: x[1])
    g = [(f"{pts[i][0]}>{pts[i + 1][0]}", pts[i + 1][1] - pts[i][1]) for i in range(len(pts) - 1)]
    if d["live"]:
        g.append((f"{pts[-1][0]}>(running)", d["age"] - pts[-1][1]))
    return g


def phase(d):
    g = gaps(d)
    return max(g, key=lambda x: x[1]) if g else ("?", 0)


def span(d):
    return d["age"] / 1000.0


byport = defaultdict(list)
for d in recs.values():
    byport[d["port"]].append(d)

stalls = sorted((r for r in att if r["dt"] >= a.threshold), key=lambda r: r["t"])
print("# Server trace joined to client stalls\n")
print(f"{len(att)} client attempts, {len(stalls)} at or above {a.threshold:g} s. Trace: {polls} polls "
      f"({full} full), {len(recs)} distinct requests seen, {slots} records, lost={lost} miss={miss}, "
      f"{workers} workers.\n")

matched = []
for r in stalls:
    lo, hi = r["t"] - a.slack, r["t"] + r["dt"] + a.slack
    best = None
    for d in byport.get(r.get("lport"), []):
        if d["n"] != r.get("conn_n"):
            continue
        end = d["wall"] + (span(d) if not d["live"] else 0) + 0.0
        if d["wall"] <= hi and (d["live"] or end >= lo):
            if best is None or abs(d["wall"] - r["t"]) < abs(best["wall"] - r["t"]):
                best = d
    matched.append((r, best))

n_hit = sum(1 for _, d in matched if d)
print(f"**{n_hit} of {len(stalls)} stalls matched to a trace record.**\n")

if matched:
    print("## Stall to worker to phase\n")
    print("| time | kind | lvl | dt s | ka | w | tcb | n | server age ms | last | phase (largest gap, ms) | polls | txn | txst | txms | path |")
    print("|---|---|--:|--:|:-:|---|---|--:|--:|---|---|--:|--:|--:|--:|---|")
    for r, d in matched:
        t = __import__("time").strftime("%H:%M:%S", __import__("time").gmtime(r["t"]))
        if d:
            ph, ms = phase(d)
            print(f"| {t} | {r['kind']} | {r['level']} | {r['dt']:.1f} | {'y' if r['ka'] else 'n'} | {d['w']} | {d['tcb']} | "
                  f"{d['n']} | {d['age']} | {d['last']} | {ph} ({ms}) | {d['polls']} | {d['txn']} | {d['txst']} | {d['txms']} | {d['path']} |")
        else:
            print(f"| {t} | {r['kind']} | {r['level']} | {r['dt']:.1f} | {'y' if r['ka'] else 'n'} | - | - | - | - | - | no record | | | | | |")

    ok = [(r, d) for r, d in matched if d]
    if ok:
        print("\n## Matched stalls by phase\n")
        c = Counter(phase(d)[0] for _, d in ok)
        print("| phase | stalls |\n|---|--:|")
        for k, v in c.most_common():
            print(f"| {k} | {v} |")
        print("\n## Matched stalls by worker\n")
        c = Counter(d["w"] for _, d in ok)
        print("| worker | stalls | ... of which in phase |\n|---|--:|---|")
        for k, v in c.most_common():
            pc = Counter(phase(d)[0] for _, d in ok if d["w"] == k)
            print(f"| {k} | {v} | {', '.join(f'{x} {y}' for x, y in pc.most_common())} |")

fulls = [d for d in recs.values() if d["full"] and d["w"] not in ("0", "000000")]
if fulls:
    print("\n## All traced requests by worker (seen in full polls)\n")
    c = Counter(d["w"] for d in fulls)
    slow = Counter(d["w"] for d in fulls if span(d) >= a.threshold)
    print("| worker | requests | at or above threshold | share of requests |\n|---|--:|--:|--:|")
    for k, v in c.most_common():
        print(f"| {k} | {v} | {slow.get(k, 0)} | {100.0 * v / len(fulls):.1f} % |")

srv = [d for d in recs.values() if span(d) >= a.threshold and d["n"] == 0 or (d["n"] > 0 and d.get("line1") is not None and d["age"] - d["line1"] >= a.threshold * 1000)]
if srv:
    print(f"\n## Server-side requests of {a.threshold:g} s or more, matched or not\n")
    c = Counter(phase(d)[0] for d in srv)
    print("| phase | requests |\n|---|--:|")
    for k, v in c.most_common():
        print(f"| {k} | {v} |")

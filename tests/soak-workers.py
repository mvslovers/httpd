#!/usr/bin/env python3
"""List the worker threads of a running HTTPD, in creation order.

Reads /.dsrv?target=MGR and the WORKERS block it points to, and prints per
worker: array index (creation order), CTHDWORK address, state, start time and
the number of connections dispatched to it.  The addresses are the ones the
request trace prints as w=, so a stall can be put next to the load its worker
carried.  Needs the /.dsrv display route.

    soak-workers.py --url URL --user USER --password PASSWORD
"""
import argparse
import base64
import re
import sys
import urllib.request


def fetch(url, auth):
    req = urllib.request.Request(url, headers={"Authorization": auth})
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read().decode("latin-1")


ap = argparse.ArgumentParser()
ap.add_argument("--url", required=True)
ap.add_argument("--user", required=True)
ap.add_argument("--password", required=True)
a = ap.parse_args()

auth = "Basic " + base64.b64encode(f"{a.user}:{a.password}".encode()).decode()
base = a.url.rstrip("/")

mgr = fetch(f"{base}/.dsrv?target=MGR", auth)
m = re.search(r"target=WORKERS&amp;m=([0-9A-Fa-f]+)|target=WORKERS&m=([0-9A-Fa-f]+)", mgr)
if not m:
    sys.exit("no WORKERS link in the MGR display")
arr = m.group(1) or m.group(2)

page = fetch(f"{base}/.dsrv?target=WORKERS&m={arr}", auth)
text = re.sub(r"<[^>]*>", " ", page)

print("| index | worker | state | started | dispatched |")
print("|--:|---|---|---|--:|")
for blk in re.split(r"Worker Thread #", text)[1:]:
    h = re.match(r"(\d+)\s+([0-9A-Fa-f]{8})", blk)
    st = re.search(r"worker->state\s+Current State\s+\d+ \((\w+)\)", blk)
    t0 = re.search(r"worker->start_time\s+Start Time\s+(.+?)\s{2,}", blk)
    dp = re.search(r"worker->dispatched\s+Dispatched Count\s+(\d+)", blk)
    if not h or not st:
        continue
    print(f"| {h.group(1)} | {h.group(2)[2:]} | {st.group(1) if st else '?'} | "
          f"{t0.group(1) if t0 else '?'} | {dp.group(1) if dp else '?'} |")

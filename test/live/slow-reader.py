#!/usr/bin/env python3
"""Live check for issue #440: a slow reader of a large static file.

Requests a static file of a known size (Content-Length, so not the chunked
path) through a receive buffer of 4096 bytes and reads it in 2048-byte pieces
with a pause between them.  The client either finishes the transfer or, with
--abandon, stops reading once that many body bytes have arrived and keeps the
connection open, which is what a client on a bad line looks like to the
server.  Run against a throwaway system only, with TRACE=n set in HTTPPRM0 and
the route for /.dsrv in place.

    slow-reader.py URL PATH [--pause 0.05] [--abandon N] [--hold SEC]
                   [--trace URL] [--expect FILE] [--deadline SEC]
                   [--clients N]

URL is http://host:port, PATH a static file of a few hundred KB below the
docroot.  Connect to the address of the system itself: a port forwarded by a
proxy takes the whole file in its own buffer and never stalls.

With --abandon each client stops reading after N body bytes and holds the
connection open for --hold seconds.  With --trace (the URL of the request
trace, http://host:port/.dsrv?target=TRACE) the client also watches its own
record there, found by its source port, and reports how long after the stop
the server's worker was free again ('live=0').  A worker that is still
pinned when the hold is over is the failure.  With --expect (the local copy of
the file) a client that read the whole body also compares it with that copy,
and a body that differs counts as 'wrong'.

Per client a line 'client K: bytes B of L, Ts, how' is printed, 'how' being
complete, wrong, cut (the server closed early), abandoned or deadline.
With --trace a line 'client K: worker freed after S s' or
'client K: worker pinned' follows.
The last line is 'RESULT <n> complete, <n> cut, <n> abandoned, <n> deadline,
<n> wrong, <n> pinned', and the exit status is 1 if any worker was pinned,
any body was wrong or any client ran into the deadline.
"""
import argparse
import re
import socket
import sys
import threading
import time
import urllib.request
from urllib.parse import urlparse

ap = argparse.ArgumentParser()
ap.add_argument('url')
ap.add_argument('path')
ap.add_argument('--pause', type=float, default=0.05)
ap.add_argument('--abandon', type=int, default=0)
ap.add_argument('--hold', type=float, default=60.0)
ap.add_argument('--trace', default='')
ap.add_argument('--expect', default='')
ap.add_argument('--deadline', type=float, default=120.0)
ap.add_argument('--clients', type=int, default=1)
a = ap.parse_args()
u = urlparse(a.url)
results = {}
pinned = {}
lock = threading.Lock()


def worker_live(port):
    """True/False for the trace record of this source port, None if absent."""
    page = urllib.request.urlopen(a.trace, timeout=10).read().decode('latin-1')
    live = None
    for line in page.splitlines():
        if re.search(r'\bport=%d\b' % port, line):
            m = re.search(r'\blive=(\d)', line)
            if m:
                live = m.group(1) == '1'
    return live


def hold_and_watch(k, s, port):
    t0 = time.time()
    while time.time() - t0 < a.hold:
        if a.trace:
            try:
                if worker_live(port) is False:
                    with lock:
                        print('client %d: worker freed after %.1f s'
                              % (k, time.time() - t0))
                    return
            except OSError:
                pass
        time.sleep(1)
    if a.trace:
        with lock:
            pinned[k] = True
            print('client %d: worker pinned after %.1f s' % (k, a.hold))


def client(k):
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
    s.settimeout(30)
    s.connect((u.hostname, u.port))
    port = s.getsockname()[1]
    s.sendall(('GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n'
               % (a.path, u.netloc)).encode())
    t0 = time.time()
    got = b''
    length = -1
    how = 'deadline'
    body = 0
    try:
        while time.time() - t0 < a.deadline:
            b = s.recv(2048)
            if not b:
                how = 'cut'
                break
            got += b
            if length < 0 and b'\r\n\r\n' in got:
                head, _, rest = got.partition(b'\r\n\r\n')
                for line in head.split(b'\r\n'):
                    if line.lower().startswith(b'content-length:'):
                        length = int(line.split(b':')[1])
                body = len(rest)
            elif length >= 0:
                body += len(b)
            if length >= 0 and body >= length:
                how = 'complete'
                if a.expect:
                    with open(a.expect, 'rb') as f:
                        if got.partition(b'\r\n\r\n')[2] != f.read():
                            how = 'wrong'
                break
            if a.abandon and body >= a.abandon:
                how = 'abandoned'
                break
            time.sleep(a.pause)
    except (socket.timeout, OSError) as e:
        how = 'cut (%s)' % type(e).__name__
    dt = time.time() - t0
    with lock:
        results[k] = how
        print('client %d: bytes %d of %d, %.1fs, %s' % (k, body, length, dt, how))
        sys.stdout.flush()
    if how == 'abandoned':
        hold_and_watch(k, s, port)
    s.close()


threads = [threading.Thread(target=client, args=(k,)) for k in range(a.clients)]
for t in threads:
    t.start()
for t in threads:
    t.join()
n = lambda w: sum(1 for v in results.values() if v.startswith(w))
print('RESULT %d complete, %d cut, %d abandoned, %d deadline, %d wrong, %d pinned'
      % (n('complete'), n('cut'), n('abandoned'), n('deadline'), n('wrong'),
         len(pinned)))
sys.exit(1 if pinned or n('deadline') or n('wrong') else 0)

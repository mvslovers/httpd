#!/usr/bin/env python3
"""Live check for issue #399: HEAD on a static file.

A response to HEAD ends at the blank line after the headers (RFC 7230 3.3.3).
Before the fix HEAD was answered 501 with a chunked body, which a client does
not read, so on a keep-alive connection the chunks became the start of the
next response.  This check speaks raw HTTP on ONE connection and looks at
every byte, because a client library hides exactly the defect it looks for.

    head-static.py URL FILE [MISSING]

URL is http://host:port, FILE the path of an existing static file below the
docroot (for example /t399.txt), MISSING the path of a file that does not
exist (default /qa-399-missing.txt).  Run against a throwaway system only.

Checks:
  - HEAD FILE, GET FILE, HEAD FILE on one connection: 200 each, no
    Transfer-Encoding on a HEAD response, no byte after a HEAD response's
    blank line, Content-Length and Content-Type of HEAD equal GET's, and
    Content-Length equal to the length of the GET body
  - HEAD MISSING: 404, no Transfer-Encoding, no body, and the connection
    still answers a following GET FILE

The last line is 'RESULT ok' or 'RESULT FAILED n'; the exit code is 0 or 1.
"""
import socket
import sys
from urllib.parse import urlparse

url, path = sys.argv[1], sys.argv[2]
missing = sys.argv[3] if len(sys.argv) > 3 else '/qa-399-missing.txt'
u = urlparse(url)
failures = []


def check(ok, what):
    print('%s  %s' % ('ok  ' if ok else 'FAIL', what))
    if not ok:
        failures.append(what)


def code(status):
    parts = status.split(' ', 2)
    return parts[1] if len(parts) > 1 else status


class Conn:
    def __init__(self):
        self.s = socket.create_connection((u.hostname, u.port), timeout=15)
        self.buf = b''

    def _fill(self):
        d = self.s.recv(65536)
        if not d:
            raise EOFError('server closed the connection')
        self.buf += d

    def request(self, method, p):
        self.s.sendall(('%s %s HTTP/1.1\r\nHost: %s\r\n\r\n'
                        % (method, p, u.netloc)).encode())
        while b'\r\n\r\n' not in self.buf:
            self._fill()
        head, _, self.buf = self.buf.partition(b'\r\n\r\n')
        lines = head.decode('latin-1').split('\r\n')
        hdr = {}
        for ln in lines[1:]:
            k, _, v = ln.partition(':')
            hdr[k.strip().lower()] = v.strip()
        return lines[0], hdr

    def body(self, n):
        while len(self.buf) < n:
            self._fill()
        b, self.buf = self.buf[:n], self.buf[n:]
        return b

    def stray(self):
        """Bytes that arrive after a response that must have ended."""
        self.s.settimeout(0.7)
        try:
            d = self.s.recv(65536)
            self.buf += d
        except socket.timeout:
            pass
        finally:
            self.s.settimeout(15)
        return self.buf


def head_response(c, label, p, expect):
    status, hdr = c.request('HEAD', p)
    check(code(status) == expect, '%s: status %s' % (label, status))
    check('transfer-encoding' not in hdr, '%s: no Transfer-Encoding' % label)
    left = c.stray()
    check(left == b'', '%s: nothing after the blank line (%r)' % (label, left[:60]))
    return hdr


try:
    c = Conn()
    h1 = head_response(c, 'HEAD 1', path, '200')

    status, g = c.request('GET', path)
    check(code(status) == '200', 'GET: status %s' % status)
    check('content-length' in g, 'GET: has Content-Length')
    n = int(g.get('content-length', '0') or 0)
    body = c.body(n)
    check(len(body) == n and n > 0, 'GET: body of %d bytes' % n)
    check(c.stray() == b'', 'GET: nothing after the body')

    h2 = head_response(c, 'HEAD 2', path, '200')
    for k in ('content-length', 'content-type'):
        check(h1.get(k) == g.get(k) and h2.get(k) == g.get(k),
              'HEAD %s equals GET %s (%s)' % (k, k, g.get(k)))

    c2 = Conn()
    head_response(c2, 'HEAD missing', missing, '404')
    status, g2 = c2.request('GET', path)
    check(code(status) == '200' and g2.get('content-length') == str(n),
          'GET after HEAD 404 on the same connection: %s' % status)
    c2.body(n)
except (EOFError, OSError) as e:
    check(False, 'connection broke: %r' % e)

if failures:
    print('RESULT FAILED %d' % len(failures))
    sys.exit(1)
print('RESULT ok')

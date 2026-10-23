#!/usr/bin/env python3
"""Live check for HTTPD065W (issue #347): a client that stops reading.

Requests a chunked response of several megabytes (a sequential data set read
through mvsMF, larger than every buffer between the worker and the client),
reads the start of it, then nothing for HOLD seconds, then drains it and
checks that it arrived whole.  Run against a throwaway system only.

    stall-giveup.py URL USER PASSWORD DSN HOLD

URL is http://host:port.  The last line is 'RESULT complete' (the zero chunk
arrived) or 'RESULT cut' (the server closed before that).  The caller looks
for HTTPD065W on the console before and after.
"""
import base64, socket, sys, time
from urllib.parse import urlparse

url, user, pw, dsn, hold = (sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4],
                            float(sys.argv[5]))
u = urlparse(url)
auth = base64.b64encode(('%s:%s' % (user, pw)).encode()).decode()
req = ('GET /zosmf/restfiles/ds/%s HTTP/1.1\r\nHost: %s\r\n'
       'X-CSRF-ZOSMF-HEADER: x\r\nAuthorization: Basic %s\r\n\r\n'
       % (dsn, u.netloc, auth)).encode()
END = b'\r\n0\r\n\r\n'

s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 2048)
s.settimeout(20)
s.connect((u.hostname, u.port))
s.sendall(req)
t0 = time.time()
data = s.recv(4096)                     # headers and the first chunk
print('response started (%d bytes), reading nothing for %.0f s' % (len(data), hold))
sys.stdout.flush()
time.sleep(hold)

try:
    while not data.endswith(END):
        b = s.recv(65536)
        if not b:
            break
        data += b
except (socket.timeout, ConnectionResetError, OSError):
    pass

whole = data.endswith(END)
print('%.1f s, %d bytes, zero chunk %s' % (time.time() - t0, len(data), 'yes' if whole else 'no'))
print('RESULT complete' if whole else 'RESULT cut')

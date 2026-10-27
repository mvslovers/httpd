#!/usr/bin/env python3
"""Live check for issue #271: job submit through mvsMF fails IEF722I
INVALID PASSWORD GIVEN while the HTTP session keeps authenticating.

Submits trivial IEFBR14 jobs through PUT /zosmf/restjobs/jobs with Basic
auth and reports, per job, whether JES2 accepted the JOB card password or
rejected it (retcode JCL ERROR with IEF722I in the job log).  The password is
never printed.  Run against a throwaway system only.

    submit-password.py URL USER PASSWORD SCENARIO [N]

URL is http://host:port.  SCENARIO:

  baseline   N submits, one after the other, one keep-alive connection per
             submit
  rst        before every submit, abandon a few connections the way a dead
             network does: request half sent, then RST; request sent, response
             never read, then RST
  silent     before every submit, open connections that send nothing and are
             then left open (server side sits in its read timeout)
  churn      before every submit, 12 parallel clients for 4 s (the worker pool
             grows to MAXTASK), then GAP seconds (default 20) of quiet
  abend      before every submit, ABENDS (default 3) requests to a module that
             abends, so workers die and are replaced (needs MOD=ABEND0C1 /abend)
  abort      before every submit, send a request with a body shorter than its
             Content-Length and RST
  stale      positive control for the detector: before submit 2, change the
             user's RACF/RAKF password behind httpd's back (ALTUSER job, then
             S RAKFUSER through the Hercules HTTP interface; env HERC_URL,
             HERC_AUTH=user:pass, NEWPW).  Credentials cached by httpd keep
             authenticating, but the JOB card password is stale, so submits
             2..N are expected to be REJECTED.  Leaves the system with the
             new password; restore it with ALTUSER after an httpd restart.

The last line is 'RESULT ok' (every job ran, none rejected) or
'RESULT rejected N/M' (N of M jobs hit IEF722I or JCL ERROR).
"""
import base64, http.client, json, os, socket, struct, sys, time
from concurrent.futures import ThreadPoolExecutor
from urllib.parse import urlparse

url, user, pw, scenario = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
count = int(sys.argv[5]) if len(sys.argv) > 5 else 10
u = urlparse(url)
auth = 'Basic ' + base64.b64encode(('%s:%s' % (user, pw)).encode()).decode()
HDR = {'Authorization': auth, 'X-CSRF-ZOSMF-HEADER': 'x'}
JCL = ("//SUBT%03d JOB (ACCT),'T271',CLASS=A,MSGCLASS=H\n"
       "//S1 EXEC PGM=IEFBR14\n")


def conn():
    return http.client.HTTPConnection(u.hostname, u.port, timeout=30)


def rst_close(s):
    s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
    s.close()


def raw(request=b'', linger_rst=True, hold=0):
    s = socket.socket()
    s.settimeout(10)
    s.connect((u.hostname, u.port))
    if request:
        s.sendall(request)
    if hold:
        time.sleep(hold)
    if linger_rst:
        rst_close(s)
    return s


def req_bytes(method, path, body=b'', length=None):
    h = '%s %s HTTP/1.1\r\nHost: %s\r\nAuthorization: %s\r\n' \
        'X-CSRF-ZOSMF-HEADER: x\r\n' % (method, path, u.netloc, auth)
    if body or length is not None:
        h += 'Content-Type: text/plain\r\nContent-Length: %d\r\n' % (
            len(body) if length is None else length)
    return (h + '\r\n').encode() + body


def burst(workers, seconds):
    """Keep `workers` requests in flight for `seconds`, so the pool grows."""
    end = time.time() + seconds

    def loop(_):
        while time.time() < end:
            try:
                c = conn()
                c.request('GET', '/zosmf/restjobs/jobs?owner=%s' % user,
                          headers=HDR)
                c.getresponse().read()
                c.close()
            except Exception:
                pass

    with ThreadPoolExecutor(workers) as ex:
        list(ex.map(loop, range(workers)))


def change_password():
    jcl = ("//ALTPW1 JOB (ACCT),'T271',CLASS=A,MSGCLASS=H,GROUP=RAKFADM\n"
           "//T EXEC PGM=IKJEFT01,REGION=4096K\n"
           "//SYSPRINT DD SYSOUT=*\n//SYSTSPRT DD SYSOUT=*\n//SYSTSIN DD *\n"
           " ALTUSER %s PASSWORD(%s)\n/*\n" % (user, os.environ['NEWPW']))
    c = conn()
    c.request('PUT', '/zosmf/restjobs/jobs', jcl.encode(),
              dict(HDR, **{'Content-Type': 'text/plain'}))
    d = json.loads(c.getresponse().read())
    c.close()
    rc, _ = outcome(d)
    if rc != 'CC 0000':
        sys.exit('ALTUSER job ended %s' % rc)
    h = urlparse(os.environ['HERC_URL'])
    c = http.client.HTTPConnection(h.hostname, h.port, timeout=20)
    c.request('GET', '/cgi-bin/tasks/syslog?command=%2FS+RAKFUSER',
              headers={'Authorization': 'Basic ' + base64.b64encode(
                  os.environ['HERC_AUTH'].encode()).decode()})
    c.getresponse().read()
    c.close()
    time.sleep(8)


def damage(n):
    keep = []
    if scenario == 'stale' and n == 2:
        change_password()
    if scenario == 'abend':
        for _ in range(int(os.environ.get('ABENDS', '3'))):
            try:
                c = conn()
                c.request('GET', '/abend')
                c.getresponse().read()
                c.close()
            except Exception:
                pass
        burst(6, 2)
    elif scenario == 'churn':
        burst(12, 4)
        time.sleep(int(os.environ.get('GAP', '20')))
    elif scenario == 'rst':
        raw(req_bytes('PUT', '/zosmf/restjobs/jobs', b'', 400)[:60])
        raw(req_bytes('GET', '/zosmf/restjobs/jobs?owner=%s' % user))
        raw(req_bytes('PUT', '/zosmf/restjobs/jobs', (JCL % 999).encode()))
    elif scenario == 'silent':
        for _ in range(4):
            keep.append(raw(linger_rst=False))
    elif scenario == 'abort':
        raw(req_bytes('PUT', '/zosmf/restjobs/jobs', (JCL % 998)[:20].encode(),
                      len(JCL % 998)))
    return keep


def submit(n):
    c = conn()
    c.request('PUT', '/zosmf/restjobs/jobs', (JCL % n).encode(),
              dict(HDR, **{'Content-Type': 'text/plain'}))
    r = c.getresponse()
    body = r.read()
    c.close()
    if r.status not in (200, 201):
        return None, 'HTTP %d' % r.status
    d = json.loads(body)
    return d, None


def outcome(d):
    name, jid = d['jobname'], d['jobid']
    for _ in range(60):
        c = conn()
        c.request('GET', '/zosmf/restjobs/jobs/%s/%s' % (name, jid), headers=HDR)
        r = c.getresponse()
        j = json.loads(r.read())
        c.close()
        if j.get('status') == 'OUTPUT':
            return j.get('retcode') or '', jid
        time.sleep(1)
    return 'TIMEOUT', jid


rejected = 0
held = []
for n in range(1, count + 1):
    held += damage(n)
    d, err = submit(n)
    if err:
        print('%3d submit refused: %s' % (n, err))
        rejected += 1
        continue
    rc, jid = outcome(d)
    bad = rc in ('JCL ERROR', 'TIMEOUT') or not rc.startswith('CC')
    rejected += bad
    print('%3d %s %s %s' % (n, jid, rc, 'REJECTED' if bad else 'ok'))
    sys.stdout.flush()
for s in held:
    s.close()

print('RESULT ok' if not rejected else 'RESULT rejected %d/%d' % (rejected, count))
sys.exit(1 if rejected else 0)

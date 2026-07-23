#!/bin/sh
# shutdown-acceptance.sh — acceptance test for mvslovers/httpd#122
#
# Verifies that stopping HTTPD (P HTTPD) AFTER a request handler has faulted
# terminates the address space cleanly — no S33E, no repeated "__CRTGET CRT
# for TCB(...) was not found in PPA(...)", no SVC dump, and a NORMAL address-
# space end: no IEF450I ... ABEND, no "$HASP310 ... TERMINATED AT END OF
# MEMORY", no SVC-dump (IEA911E). The "$HASP395 ... ENDED" / IEF404I end
# records only confirm the end was CAPTURED in the window — they appear on
# clean and abnormal ends alike and do not themselves prove health.
#
# Background: on shutdown, worker teardown DETACHes tasks without proving they
# terminated (S33E), and the worker recovery ESTAE runs the C runtime under a
# torn-down CRT (__CRTGET spam). The code fix lives in libc370. This test is
# the httpd-side acceptance criterion for #122 — it observes the failure in
# httpd's own address space.
#
# EXPECTED RESULT ON THE CURRENT (PRE-FIX) BUILD: FAIL.
# A PASS today does not mean "green" — it means the provocation did not leave a
# worker in the post-fault state #122 needs. See the result banner at the end.
#
# ---------------------------------------------------------------------------
# THINGS THAT SILENTLY BREAK THIS TEST — read before editing:
#
#   1. WINDOWED ASSERTION. CONSOLE_LOG is typically an append-only Hercules
#      hardcopy log that ALREADY contains S33E / __CRTGET lines from earlier
#      real crashes. Grepping the whole file would report FAIL forever — even
#      after the fix — and look exactly like the fix not working. This script
#      records the byte offset of CONSOLE_LOG immediately before provoking and
#      asserts ONLY over bytes appended after that point. Do not remove this.
#
#   2. FAULT MARKER. Proof-of-fault is HTTPD062E *or* MVSMF99E. httpd emits
#      HTTPD062E for a CORE abend caught by the worker's try(serve_client)
#      (httpd.c:682) — this is what /abend produces. An mvsMF CGI abend is
#      caught by mvsMF's inner ESTAE instead and logs MVSMF99E, so the worker
#      try() returns 0 and HTTPD062E never fires. Accepting either keeps the
#      test valid whether ABEND_PATH is core (/abend) or a CGI endpoint.
#
#   3. ABNORMAL ADDRESS-SPACE END. A clean in-flight run (no S33E/__CRTGET/SVC
#      dump) is NOT enough. After the 2026-07-23 recovery-exit relink the
#      symptom moved from S33E to an abnormal address-space end (IEF450I ...
#      ABEND SA03). This test therefore also fails on "IEF450I ... ABEND",
#      "$HASP310 ... TERMINATED AT END OF MEMORY", and SVC-dump markers (IEA911E,
#      via the svcdump group). The "$HASP395 ... ENDED" / IEF404I records do NOT
#      discriminate health: they appear on clean AND abnormal ends alike (S33E,
#      SA03, EOM). They are used only to confirm the end was CAPTURED in the
#      window (end_seen), so a truncated log is not mistaken for a clean shutdown.
#      The failure patterns are kept LOOSE (not scoped to $JOBNAME) so a stray
#      match only over-reports FAIL; the end-capture markers are scoped TIGHT to
#      $JOBNAME so a miss downgrades to INCONCLUSIVE. Both err toward safe.
#
#   4. HTTPD060I / HTTPD002I ARE DIAGNOSTIC, NOT A GATE. Worker- and server-
#      shutdown WTOs corroborate an orderly drain but never decide the verdict:
#      the authoritative signal is the address-space end (note 3). The SA03 run
#      produced zero HTTPD060I, so a PASS with zero of them emits a CAUTION, not
#      a FAIL. Do not turn these into a hard requirement.
#
#   5. PROVOCATION MODE decides WHICH shutdown path you test. Default
#      PROVOKE=abend faults handlers -- that is the recovery-drain path
#      (libc370#9), NOT a worker parked in a long poll. The #179 / SA03 crash
#      needs a worker still IN FLIGHT in a long poll when P HTTPD lands, which a
#      fast fault cannot create; PROVOKE=longpoll launches $LONGPOLL_N concurrent
#      blocking long-polls (LONGPOLL_CMD, e.g. an unsol-detect-sync console PUT)
#      and proves >=1 is still parked at stop. Proof-of-scenario is that in-flight
#      count, NOT a fault marker -- a clean parked poll faults nothing. A green
#      abend run does NOT establish the #179 fix; use longpoll for that.
# ---------------------------------------------------------------------------
# Configuration (override via environment or a sourced .env)
# ---------------------------------------------------------------------------
#   HTTPD_HOST     host serving the HTTPD listener        (default: MBT_MVS_HOST or 127.0.0.1)
#   HTTPD_PORT     HTTPD listener port to attack + poll   (default: 8080)
#   ABEND_PATH     path whose handler faults              (default: /abend)
#   ABEND_HITS     how many times to hit it (poison pool) (default: 12)
#   FAULT_MARK     regex proving a handler faulted        (default: HTTPD062E|MVSMF99E)
#   HTTPD_AUTH     optional "user:pass" if LOGIN gates GET (default: unset)
#   WAIT_SECS      max seconds to wait for the AS to end   (default: 120)
#   JOBNAME        MVS jobname of the HTTPD started task   (default: HTTPD)
#
#   PROVOKE        pre-stop scenario: abend | longpoll     (default: abend)
#                  abend    -> fault handlers (recovery-drain path; libc370#9).
#                  longpoll -> park workers in an in-flight long poll at P HTTPD
#                              (the #179 / SA03 path a fast fault cannot create).
#   LONGPOLL_CMD   PROVOKE=longpoll, REQUIRED: one blocking long-poll request
#                  (e.g. an unsol-detect-sync console PUT). Run $LONGPOLL_N times,
#                  concurrently, in the background — see tests/README.md.
#   LONGPOLL_N     PROVOKE=longpoll: concurrent long-polls              (default: 4)
#   LONGPOLL_SETTLE PROVOKE=longpoll: seconds to let them park pre-stop (default: 3)
#
#   STOP_ADAPTER   how "P HTTPD" is issued: manual | cmd   (default: manual)
#   STOP_CMD       command line to issue P HTTPD when STOP_ADAPTER=cmd
#                  (e.g. a Hercules console pipe, or a one-shot operator-command
#                   REST call — see tests/README.md). Must return promptly.
#
#   CONSOLE_LOG    REQUIRED. Path to the MVS console / hardcopy log (SYSLOG
#                  extract, or the Hercules hardcopy log). The script asserts
#                  over the slice appended during THIS run. It must cover the
#                  window from "P HTTPD" through address-space termination.
# ---------------------------------------------------------------------------

set -u

# --- load .env if present (for HTTPD_HOST default only) ---------------------
if [ -f "${ENV_FILE:-.env}" ]; then
    # shellcheck disable=SC1090
    . "${ENV_FILE:-.env}" 2>/dev/null || true
fi

HTTPD_HOST=${HTTPD_HOST:-${MBT_MVS_HOST:-127.0.0.1}}
HTTPD_PORT=${HTTPD_PORT:-8080}
ABEND_PATH=${ABEND_PATH:-/abend}
ABEND_HITS=${ABEND_HITS:-12}
FAULT_MARK=${FAULT_MARK:-HTTPD062E|MVSMF99E}
WAIT_SECS=${WAIT_SECS:-120}
STOP_ADAPTER=${STOP_ADAPTER:-manual}
JOBNAME=${JOBNAME:-HTTPD}
PROVOKE=${PROVOKE:-abend}
LONGPOLL_N=${LONGPOLL_N:-4}
LONGPOLL_SETTLE=${LONGPOLL_SETTLE:-3}

fail() { printf '%s\n' "$*" >&2; exit 2; }

[ -n "${CONSOLE_LOG:-}" ] || fail "CONSOLE_LOG is required (path to the captured console/hardcopy log)."
[ -r "${CONSOLE_LOG}" ]   || fail "CONSOLE_LOG '$CONSOLE_LOG' is not readable."

CURL="curl -s -S -o /dev/null -w %{http_code} --max-time 10"
[ -n "${HTTPD_AUTH:-}" ] && CURL="$CURL -u $HTTPD_AUTH"
BASE="http://${HTTPD_HOST}:${HTTPD_PORT}"

echo "== httpd#122 shutdown acceptance =="
echo "   target  : $BASE"
echo "   provoke : $PROVOKE"
if [ "$PROVOKE" = abend ]; then
    echo "   abend   : $ABEND_PATH x $ABEND_HITS"
else
    echo "   longpoll: $LONGPOLL_N concurrent, settle ${LONGPOLL_SETTLE}s"
fi
echo "   stop    : $STOP_ADAPTER"
echo "   log     : $CONSOLE_LOG"
echo

# --- 1. mark the log window START (byte offset) — see note #1 above ---------
# Everything already in CONSOLE_LOG (including prior real crashes) is BEFORE
# this offset and is excluded from the assertion.
log_offset=$(wc -c < "$CONSOLE_LOG")
log_offset=$(printf '%s' "$log_offset" | tr -d ' ')
echo "-- console-log window starts at byte offset $log_offset --"
echo

# --- 2. provoke: put the worker pool into the pre-stop state under test -----
# abend    -> handlers fault (fast; the recovery-drain path, libc370#9).
# longpoll -> workers PARKED in an in-flight long poll still running when P HTTPD
#             lands — the #179 / SA03 path a fast fault cannot reproduce.
LONGPOLL_PIDS=""
inflight=0
case "$PROVOKE" in
abend)
    echo "-- provoking handler abend ($ABEND_PATH x $ABEND_HITS) --"
    faulted=0
    i=0
    while [ "$i" -lt "$ABEND_HITS" ]; do
        i=$((i + 1))
        code=$($CURL "${BASE}${ABEND_PATH}" 2>/dev/null) || code="conn-reset"
        # A faulting worker drops the connection or returns 5xx; a clean 200/404
        # means the handler did NOT fault (wrong path / already handled otherwise).
        case "$code" in
            5*|conn-reset|000) faulted=$((faulted + 1)) ;;
        esac
        printf '   hit %2d -> %s\n' "$i" "$code"
    done
    echo "   handler-fault responses: $faulted / $ABEND_HITS"
    ;;
longpoll)
    [ -n "${LONGPOLL_CMD:-}" ] || fail "PROVOKE=longpoll requires LONGPOLL_CMD (one blocking long-poll request)."
    echo "-- launching $LONGPOLL_N concurrent long-polls (must be in flight at P HTTPD) --"
    trap '[ -n "$LONGPOLL_PIDS" ] && kill $LONGPOLL_PIDS 2>/dev/null' EXIT INT TERM
    i=0
    while [ "$i" -lt "$LONGPOLL_N" ]; do
        i=$((i + 1))
        sh -c "$LONGPOLL_CMD" >/dev/null 2>&1 &
        LONGPOLL_PIDS="$LONGPOLL_PIDS $!"
    done
    # let the requests reach and PARK in the poll loop before we stop the server
    sleep "$LONGPOLL_SETTLE"
    for p in $LONGPOLL_PIDS; do
        kill -0 "$p" 2>/dev/null && inflight=$((inflight + 1))
    done
    echo "   long-polls still in flight after ${LONGPOLL_SETTLE}s: $inflight / $LONGPOLL_N"
    ;;
*) fail "unknown PROVOKE '$PROVOKE' (use abend|longpoll)." ;;
esac
echo

# --- 3. issue P HTTPD -------------------------------------------------------
echo "-- issuing P HTTPD --"
case "$STOP_ADAPTER" in
    cmd)
        [ -n "${STOP_CMD:-}" ] || fail "STOP_ADAPTER=cmd requires STOP_CMD."
        echo "   \$ $STOP_CMD"
        sh -c "$STOP_CMD" || fail "STOP_CMD failed."
        ;;
    manual)
        echo "   >>> Issue  P HTTPD  at the MVS operator console now."
        echo "   >>> Ensure it is written to CONSOLE_LOG ($CONSOLE_LOG)."
        printf "   >>> Press Enter once the address space has ended... "
        read ack_dummy
        ;;
    *) fail "unknown STOP_ADAPTER '$STOP_ADAPTER' (use manual|cmd)." ;;
esac
echo

# --- 4. wait for the address space to end (listener stops accepting) --------
# #122 shows ~16s of STIMER silence before termination; give it room.
echo "-- waiting up to ${WAIT_SECS}s for the listener to stop --"
ended=0
w=0
while [ "$w" -lt "$WAIT_SECS" ]; do
    if ! $CURL "${BASE}/" >/dev/null 2>&1; then
        ended=1; break
    fi
    w=$((w + 3)); sleep 3
done
[ "$ended" -eq 1 ] && echo "   listener closed after ~${w}s" || echo "   listener still up after ${WAIT_SECS}s"
echo

# --- 5. assert over the console-log slice captured for THIS run — note #1 ---
window=$(mktemp 2>/dev/null || echo "/tmp/httpd122.$$")
tail -c +"$((log_offset + 1))" "$CONSOLE_LOG" > "$window" 2>/dev/null || cp "$CONSOLE_LOG" "$window"

# proof the provocation actually faulted a worker — HTTPD062E (core) or MVSMF99E (CGI)
faultmarks=$(grep -Ec "$FAULT_MARK" "$window" 2>/dev/null || true)
# in-flight crash signatures from #122
s33e=$(grep -c "S33E" "$window" 2>/dev/null || true)
crtget=$(grep -Ec "__CRTGET CRT for TCB.*was not found in PPA" "$window" 2>/dev/null || true)
svcdump=$(grep -Eic "SVC dump|SDUMP|IEA911E|IEA794I" "$window" 2>/dev/null || true)
# abnormal address-space END — the residual after the 2026-07-23 relink (S33E -> SA03).
# The svcdump group above (IEA911E/...) is the third abnormal-end discriminator.
# LOOSE (not scoped to $JOBNAME): a stray match only over-reports FAIL, the safe way. Note 3.
abend=$(grep -Ec "IEF450I.*ABEND" "$window" 2>/dev/null || true)
hasp310=$(grep -Ec "[$]HASP310.*TERMINATED AT END OF MEMORY" "$window" 2>/dev/null || true)
# address-space END records — "$HASP395 ... ENDED" / IEF404I, both for $JOBNAME. These are
# WINDOW-COMPLETENESS evidence only (see end_seen), NOT health signals — both appear on
# clean AND abnormal ends. TIGHT to $JOBNAME so a mismatch downgrades to INCONCLUSIVE,
# not a false PASS. Note 3.
hasp395=$(grep -Ec "[$]HASP395[[:space:]]+${JOBNAME}[[:space:]].*ENDED" "$window" 2>/dev/null || true)
ief404=$(grep -Ec "IEF404I[[:space:]]+${JOBNAME}[[:space:]].*ENDED" "$window" 2>/dev/null || true)
# clean-shutdown WTOs (httpd.c:335 / :701) — DIAGNOSTIC ONLY, never a gate. Note 4.
clean=$(grep -Ec "HTTPD002I Server is SHUTDOWN|HTTPD060I SHUTDOWN worker" "$window" 2>/dev/null || true)

: "${faultmarks:=0}" "${s33e:=0}" "${crtget:=0}" "${svcdump:=0}" "${clean:=0}"
: "${abend:=0}" "${hasp310:=0}" "${hasp395:=0}" "${ief404:=0}"

# end_seen = the address-space END was CAPTURED in the window (NOT that it ended
# normally). "$HASP395 ... ENDED" and IEF404I both appear on clean AND abnormal ends,
# so they cannot judge health — IEF450I / $HASP310 / SVC dump do that. OR because either
# record alone proves the end was captured; requiring both would falsely flag a healthy
# run when JES config omits one.
if [ "$hasp395" -ne 0 ] || [ "$ief404" -ne 0 ]; then end_seen=1; else end_seen=0; fi

# proof the intended scenario actually ran (mode-specific):
#   abend    -> a handler faulted (fault marker present in the window)
#   longpoll -> at least one long-poll was still parked when P HTTPD landed
case "$PROVOKE" in
abend)    provoked=$faultmarks; provoke_desc="fault marker ($FAULT_MARK)" ;;
longpoll) provoked=$inflight;   provoke_desc="in-flight long-poll at stop" ;;
esac

echo "-- assertion over captured console window --"
echo "   provocation ($PROVOKE) proof          : $provoked  [$provoke_desc]"
echo "   S33E                                  : $s33e"
echo "   __CRTGET ... not found in PPA         : $crtget"
echo "   SVC dump markers                      : $svcdump"
echo "   IEF450I ... ABEND (abnormal AS end)   : $abend"
echo "   \$HASP310 TERMINATED AT END OF MEMORY  : $hasp310"
echo "   AS end captured (\$HASP395/IEF404I)    : $end_seen"
echo "   clean-shutdown WTOs (diagnostic)      : $clean"
echo

rm -f "$window"

# --- 6. verdict -------------------------------------------------------------
if [ "$provoked" -eq 0 ]; then
    cat <<EOF
RESULT: INCONCLUSIVE
No proof the intended scenario ran (${provoke_desc} absent), so this run did NOT
exercise the shutdown path under test.
  abend    : no handler faulted — check ABEND_PATH, LOGIN gating (HTTPD_AUTH),
             and that CONSOLE_LOG captured this run.
  longpoll : no long-poll was still in flight at P HTTPD — the requests finished
             too fast. Raise the endpoint timeout, LONGPOLL_N, or LONGPOLL_SETTLE
             so a worker is genuinely parked at stop.
Then re-run.
EOF
    exit 3
fi

if [ "$s33e" -ne 0 ] || [ "$crtget" -ne 0 ] || [ "$svcdump" -ne 0 ] \
   || [ "$abend" -ne 0 ] || [ "$hasp310" -ne 0 ]; then
    cat <<EOF
RESULT: FAIL
The address space did not terminate cleanly after the provocation + P HTTPD.
A crash signature (S33E / __CRTGET spam / SVC dump) and/or an ABNORMAL address-
space end (IEF450I ... ABEND, or "\$HASP310 ... TERMINATED AT END OF MEMORY")
appeared in the window — see the counts above.
After the recovery-exit fix (libc370#9) the in-flight crash spam is gone, but a
worker that fails to quiesce can still drive an abnormal end (SA03); that path
is caught here by the IEF450I / \$HASP310 assertion. Clean requires BOTH no
crash spam AND a normal address-space end.
EOF
    exit 1
fi

if [ "$end_seen" -eq 0 ]; then
    cat <<EOF
RESULT: INCONCLUSIVE
No crash or abnormal-end signature appeared, but the window does not contain the
address-space END records for $JOBNAME (neither "\$HASP395 ... ENDED" nor
IEF404I). The end was not captured, so a clean shutdown and a truncated log look
identical here — PASS cannot be asserted. Likely causes: CONSOLE_LOG was cut
before the AS ended, WAIT_SECS expired early, or JOBNAME does not match this STC.
Extend the capture or fix JOBNAME and re-run.
EOF
    exit 3
fi

cat <<EOF
RESULT: PASS
The provocation ran ($provoke_desc) yet shutdown was clean: no S33E, no
__CRTGET spam, no SVC dump, and no abnormal address-space end (no IEF450I ABEND,
no \$HASP310 EOM). The address-space end was captured in the window, so this is a
real clean termination and not a truncated log.
CAUTION: a PASS on a build that still carries the #122 defect is suspicious.
Confirm this build actually links the libc370 fix; if it does not, verify that
$ABEND_PATH reproduces #122's post-fault worker state before trusting this green.
EOF
if [ "$clean" -eq 0 ]; then
    cat <<EOF
CAUTION: not one HTTPD002I/HTTPD060I orderly-shutdown WTO appeared, yet the
address space ended normally. These corroborate a clean drain but do not gate
the result (the authoritative gate is the address-space end above). Zero of
them alongside a normal end is unusual — confirm the workers drained rather
than being terminated silently.
EOF
fi
exit 0

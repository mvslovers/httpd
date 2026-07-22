#!/bin/sh
# shutdown-acceptance.sh — acceptance test for mvslovers/httpd#122
#
# Verifies that stopping HTTPD (P HTTPD) AFTER a request handler has faulted
# terminates the address space cleanly — no S33E, no repeated "__CRTGET CRT
# for TCB(...) was not found in PPA(...)", no SVC dump.
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
# TWO THINGS THAT SILENTLY BREAK THIS TEST — read before editing:
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

fail() { printf '%s\n' "$*" >&2; exit 2; }

[ -n "${CONSOLE_LOG:-}" ] || fail "CONSOLE_LOG is required (path to the captured console/hardcopy log)."
[ -r "${CONSOLE_LOG}" ]   || fail "CONSOLE_LOG '$CONSOLE_LOG' is not readable."

CURL="curl -s -S -o /dev/null -w %{http_code} --max-time 10"
[ -n "${HTTPD_AUTH:-}" ] && CURL="$CURL -u $HTTPD_AUTH"
BASE="http://${HTTPD_HOST}:${HTTPD_PORT}"

echo "== httpd#122 shutdown acceptance =="
echo "   target : $BASE"
echo "   abend  : $ABEND_PATH x $ABEND_HITS"
echo "   stop   : $STOP_ADAPTER"
echo "   log    : $CONSOLE_LOG"
echo

# --- 1. mark the log window START (byte offset) — see note #1 above ---------
# Everything already in CONSOLE_LOG (including prior real crashes) is BEFORE
# this offset and is excluded from the assertion.
log_offset=$(wc -c < "$CONSOLE_LOG")
log_offset=$(printf '%s' "$log_offset" | tr -d ' ')
echo "-- console-log window starts at byte offset $log_offset --"
echo

# --- 2. provoke: fault the handler enough times to poison the worker pool ---
echo "-- provoking handler abend ($ABEND_PATH) --"
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
# failure signatures from #122
s33e=$(grep -c "S33E" "$window" 2>/dev/null || true)
crtget=$(grep -Ec "__CRTGET CRT for TCB.*was not found in PPA" "$window" 2>/dev/null || true)
svcdump=$(grep -Eic "SVC dump|SDUMP|IEA911E|IEA794I" "$window" 2>/dev/null || true)
# clean-shutdown positive markers (httpd.c:335 / :701)
clean=$(grep -Ec "HTTPD002I Server is SHUTDOWN|HTTPD060I SHUTDOWN worker" "$window" 2>/dev/null || true)

: "${faultmarks:=0}" "${s33e:=0}" "${crtget:=0}" "${svcdump:=0}" "${clean:=0}"

echo "-- assertion over captured console window --"
echo "   fault markers ($FAULT_MARK) : $faultmarks"
echo "   S33E                                  : $s33e"
echo "   __CRTGET ... not found in PPA         : $crtget"
echo "   SVC dump markers                      : $svcdump"
echo "   clean-shutdown markers                : $clean"
echo

rm -f "$window"

# --- 6. verdict -------------------------------------------------------------
if [ "$faultmarks" -eq 0 ]; then
    cat <<EOF
RESULT: INCONCLUSIVE
The console window shows no handler-fault marker ($FAULT_MARK), so no handler
faulted. This test did NOT exercise #122. Check ABEND_PATH, that GET is not
gated by LOGIN (set HTTPD_AUTH), and that CONSOLE_LOG really captured this run,
then re-run.
EOF
    exit 3
fi

if [ "$s33e" -ne 0 ] || [ "$crtget" -ne 0 ] || [ "$svcdump" -ne 0 ]; then
    cat <<EOF
RESULT: FAIL
The address space did not terminate cleanly after a faulted handler + P HTTPD.
This is the EXPECTED result on the current (pre-libc370-fix) build — it is the
#122 bug reproduced. It becomes PASS only after the libc370 recovery/DETACH fix
relinks into the httpd build.
EOF
    exit 1
fi

cat <<EOF
RESULT: PASS
A handler faulted ($FAULT_MARK present) yet shutdown was clean: no S33E, no
__CRTGET spam, no SVC dump.
CAUTION: a PASS on a build that still carries the #122 libc370 defect is
suspicious. Confirm this build actually links the libc370 fix; if it does not,
verify that $ABEND_PATH reproduces #122's post-fault worker state before
trusting this green.
EOF
exit 0

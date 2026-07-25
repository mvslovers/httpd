#!/bin/sh
# Convenience runner for tests/shutdown-acceptance.sh in longpoll mode
# (mvsmf#179 / httpd#122). It parks concurrent long-polls so a worker is still
# in flight when P HTTPD lands -- the scenario a fast handler fault cannot
# reproduce, and the only one that actually exercises the #179 quiesce fix.
#
# It is a FILE on purpose: no interactive multi-line command whose line
# continuation a stray space after a backslash can silently break.
#
# Usage:
#   sh tests/run-shutdown-longpoll.sh           real run: prompts for P HTTPD
#   sh tests/run-shutdown-longpoll.sh check      non-interactive setup check
#                                                (no P HTTPD; confirms polls park)
#
# Configure via environment (all optional except CONSOLE_LOG + credentials):
#   CONSOLE_LOG     REQUIRED: MVS console/hardcopy log the test asserts over.
#   HTTPD_HOST      HTTPD listener host                      (default: localhost)
#   HTTPD_PORT      HTTPD listener port                      (default: 8080)
#   CONSOLE_NAME    z/OSMF console name                      (default: defcn)
#   HTTPD_AUTH      API creds "user:pass"; if unset, falls back to
#                   MBT_MVS_USER / MBT_MVS_PASS from ./.env
#   JOBNAME         HTTPD STC jobname                        (default: HTTPD)
#   LONGPOLL_N      concurrent long-polls (<= MAXTASK-1)     (default: 4)
#   LONGPOLL_SETTLE seconds to let them park                (default: 3)
set -u

HOST="${HTTPD_HOST:-localhost}"
PORT="${HTTPD_PORT:-8080}"
CONSOLE="${CONSOLE_NAME:-defcn}"
: "${CONSOLE_LOG:?set CONSOLE_LOG to the MVS console/hardcopy log path}"

# API auth: HTTPD_AUTH=user:pass, else the deploy creds from ./.env.
if [ -z "${HTTPD_AUTH:-}" ]; then
    [ -f ./.env ] && . ./.env       # './' so dash reads the CWD, not $PATH
    HTTPD_AUTH="${MBT_MVS_USER:?set HTTPD_AUTH, or MBT_MVS_USER in .env}:${MBT_MVS_PASS:?set HTTPD_AUTH, or MBT_MVS_PASS in .env}"
fi

# JSON body via a temp file -> no quoting nightmare in LONGPOLL_CMD. The unsol-key
# must NOT occur during the poll (do not use a key the command itself emits), so
# the worker blocks the full unsol-detect-timeout and is still parked at P HTTPD.
body=$(mktemp)
trap 'rm -f "$body"' EXIT INT TERM
cat > "$body" <<'JSON'
{"cmd":"D T","unsol-key":"ZZNOMATCHZZ","unsol-detect-sync":"Y","unsol-detect-timeout":"60"}
JSON

export ENV_FILE=/dev/null           # config resolved above; skip the test's own .env source
export CONSOLE_LOG
export HTTPD_HOST="$HOST" HTTPD_PORT="$PORT"
export JOBNAME="${JOBNAME:-HTTPD}"
export PROVOKE=longpoll
export LONGPOLL_N="${LONGPOLL_N:-4}"
export LONGPOLL_SETTLE="${LONGPOLL_SETTLE:-3}"
export LONGPOLL_CMD="curl -s -o /dev/null -u $HTTPD_AUTH -X PUT -H 'Content-Type: application/json' -d @$body http://$HOST:$PORT/zosmf/restconsoles/consoles/$CONSOLE"

if [ "${1:-}" = check ]; then       # non-interactive: prove the polls park, no real stop
    export STOP_ADAPTER=cmd STOP_CMD=true WAIT_SECS="${WAIT_SECS:-2}"
fi

dir=$(dirname "$0")
sh "$dir/shutdown-acceptance.sh"

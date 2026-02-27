#!/bin/bash
# lnkhttpd.sh - Iterative link script for HTTPD load module
set -euo pipefail

source /Users/mike/repos/mvs/httpd/.env

BASE_URL="http://${MVS_HOST}:${MVS_PORT}/zosmf/restjobs/jobs"
JOBNAME="LKDHTTPD"

# --- LUA370 INCLUDE statements ---
LUA_INCLUDES=""
for m in LAPI LAUXLIB LSTATE LINIT LCODE LDEBUG LFUNC LGC LDUMP LMEM LOBJECT LOPCODES LSTRING LTM LTABLE LUNDUMP LVM LLEX LZIO LDO LPARSER LBASELIB LCOROLIB LDBLIB LIOLIB LMATHLIB LOSLIB LOADLIB LSTRLIB LTABLIB LUTF8LIB; do
  LUA_INCLUDES="${LUA_INCLUDES}  INCLUDE LUA370(${m})\n"
done

# --- Build JCL ---
JCL=$(cat <<ENDJCL
//${JOBNAME} JOB CLASS=${MVS_JOBCLASS},REGION=0K,MSGCLASS=${MVS_MSGCLASS},
//            MSGLEVEL=(1,1),NOTIFY=DUMMY
//*
//LINK     EXEC PGM=IEWL,PARM='LIST,MAP,XREF,RENT,SIZE=(512000,131072)'
//SYSPRINT DD SYSOUT=*
//SYSLIB   DD DISP=SHR,DSN=CRENT370.NCALIB,DCB=BLKSIZE=32760
//         DD DISP=SHR,DSN=UFS370.NCALIB
//         DD DISP=SHR,DSN=MQTT370.NCALIB
//         DD DISP=SHR,DSN=${MVS_USER}.HTTPD.NCALIB
//LUA370   DD DISP=SHR,DSN=LUA370.NCALIB
//SYSLMOD  DD DISP=SHR,DSN=${MVS_USER}.HTTPD.LOAD
//SYSUT1   DD UNIT=SYSALLDA,SPACE=(CYL,(2,1))
//SYSLIN   DD DDNAME=SYSIN
//SYSIN    DD *
  INCLUDE SYSLIB(@@CRT1)
  INCLUDE SYSLIB(HTTPD)
$(echo -e "${LUA_INCLUDES}")  ENTRY @@CRT0
  NAME HTTPD(R)
/*
//*
ENDJCL
)

# --- Strip CR, submit ---
TEMPFILE=$(mktemp)
trap 'rm -f "$TEMPFILE"' EXIT
echo "$JCL" | sed 's/\r$//' > "$TEMPFILE"

echo "=== Submitting link job ==="
RESULT=$(curl -s -S --connect-timeout 10 --max-time 60 -X PUT \
  -u "${MVS_USER}:${MVS_PASS}" \
  -H "Content-Type: text/plain" \
  -H "X-IBM-Intrdr-Mode: TEXT" \
  -H "X-IBM-Intrdr-Lrecl: 80" \
  -H "X-IBM-Intrdr-Recfm: F" \
  --data-binary @"$TEMPFILE" \
  "$BASE_URL" 2>&1)

JOBID=$(echo "$RESULT" | python3 -c "import sys,json; print(json.load(sys.stdin).get('jobid',''))" 2>/dev/null) || true
RETCODE=$(echo "$RESULT" | python3 -c "import sys,json; print(json.load(sys.stdin).get('retcode',''))" 2>/dev/null) || true

if [ -z "$JOBID" ]; then
  echo "Submit failed:"
  echo "$RESULT"
  exit 1
fi

echo "$JOBNAME ($JOBID) submitted"

# --- Poll for completion ---
while [ -z "$RETCODE" ] || [ "$RETCODE" = "None" ] || [ "$RETCODE" = "null" ]; do
  sleep 2
  STATUS=$(curl -s --connect-timeout 10 --max-time 30 \
    -u "${MVS_USER}:${MVS_PASS}" "$BASE_URL/$JOBNAME/$JOBID" 2>&1)
  RETCODE=$(echo "$STATUS" | python3 -c "import sys,json; print(json.load(sys.stdin).get('retcode',''))" 2>/dev/null) || true
done

echo "$JOBNAME ($JOBID) RC=$RETCODE"

# --- Show unresolved symbols if any ---
SYSPRINT=$(curl -s -u "${MVS_USER}:${MVS_PASS}" \
  "$BASE_URL/$JOBNAME/$JOBID/files/102/records" 2>&1)

UNRESOLVED=$(echo "$SYSPRINT" | grep '^IEW0132  [A-Z@#$]' | awk '{print $2}' | sort -u || true)
if [ -n "$UNRESOLVED" ]; then
  COUNT=$(echo "$UNRESOLVED" | wc -l | tr -d ' ')
  echo ""
  echo "=== $COUNT unresolved symbols (IEW0132) ==="
  echo "$UNRESOLVED"
else
  echo "=== No unresolved symbols ==="
fi

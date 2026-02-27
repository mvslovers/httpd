#!/bin/bash
# lnkall.sh - Link all httpd load modules
set -uo pipefail

source /Users/mike/repos/mvs/httpd/.env

BASE_URL="http://${MVS_HOST}:${MVS_PORT}/zosmf/restjobs/jobs"

# Lua370 members for modules that need them
LUA_MEMBERS="LAPI LAUXLIB LBASELIB LCODE LCOROLIB LDBLIB LDEBUG LDO LDUMP LFUNC LGC LINIT LIOLIB LLEX LMATHLIB LMEM LOADLIB LOBJECT LOPCODES LOSLIB LPARSER LSTATE LSTRING LSTRLIB LTABLE LTABLIB LTM LUNDUMP LUTF8LIB LVM LZIO"

# Modules that need Lua370
NEEDS_LUA="HTTPD HTTPLUA HTTPLUAX"

# All load modules (excluding FORMAT which comes from UFS370)
MODULES="ABEND0C1 HELLO HTTPD HTTPDM HTTPDMTT HTTPDSL HTTPDSRV HTTPJES2 HTTPLUA HTTPLUAX HTTPREXX HTTPSAY HTTPTEST"

link_module() {
  local MOD="$1"
  local JOBNAME="LKD$(echo "$MOD" | cut -c1-5)"
  JOBNAME="${JOBNAME:0:8}"

  # Build INCLUDE statements
  local INCLUDES="  INCLUDE SYSLIB(@@CRT1)\n  INCLUDE SYSLIB(${MOD})"

  # Add Lua370 INCLUDEs if needed
  local LUA_DD=""
  if echo " $NEEDS_LUA " | grep -q " $MOD "; then
    LUA_DD="//LUA370   DD DISP=SHR,DSN=LUA370.NCALIB"
    for m in $LUA_MEMBERS; do
      INCLUDES="${INCLUDES}\n  INCLUDE LUA370(${m})"
    done
  fi

  # Build JCL
  local JCL
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
${LUA_DD}
//SYSLMOD  DD DISP=SHR,DSN=${MVS_USER}.HTTPD.LOAD
//SYSUT1   DD UNIT=SYSALLDA,SPACE=(CYL,(2,1))
//SYSLIN   DD DDNAME=SYSIN
//SYSIN    DD *
$(echo -e "${INCLUDES}")
  ENTRY @@CRT0
  NAME ${MOD}(R)
/*
//*
ENDJCL
)

  # Submit
  local TEMPFILE
  TEMPFILE=$(mktemp)
  echo "$JCL" | sed 's/\r$//' > "$TEMPFILE"

  local RESULT
  RESULT=$(curl -s -S --connect-timeout 10 --max-time 60 -X PUT \
    -u "${MVS_USER}:${MVS_PASS}" \
    -H "Content-Type: text/plain" \
    -H "X-IBM-Intrdr-Mode: TEXT" \
    -H "X-IBM-Intrdr-Lrecl: 80" \
    -H "X-IBM-Intrdr-Recfm: F" \
    --data-binary @"$TEMPFILE" \
    "$BASE_URL" 2>&1)
  rm -f "$TEMPFILE"

  local JOBID
  JOBID=$(echo "$RESULT" | python3 -c "import sys,json; print(json.load(sys.stdin).get('jobid',''))" 2>/dev/null) || true
  local RETCODE
  RETCODE=$(echo "$RESULT" | python3 -c "import sys,json; print(json.load(sys.stdin).get('retcode',''))" 2>/dev/null) || true

  if [ -z "$JOBID" ]; then
    printf "  %-10s SUBMIT FAILED\n" "$MOD"
    return 1
  fi

  # Poll
  while [ -z "$RETCODE" ] || [ "$RETCODE" = "None" ] || [ "$RETCODE" = "null" ]; do
    sleep 2
    local STATUS
    STATUS=$(curl -s --connect-timeout 10 --max-time 30 \
      -u "${MVS_USER}:${MVS_PASS}" "$BASE_URL/$JOBNAME/$JOBID" 2>&1)
    RETCODE=$(echo "$STATUS" | python3 -c "import sys,json; print(json.load(sys.stdin).get('retcode',''))" 2>/dev/null) || true
  done

  # Check for unresolved
  local SYSPRINT
  SYSPRINT=$(curl -s -u "${MVS_USER}:${MVS_PASS}" \
    "$BASE_URL/$JOBNAME/$JOBID/files/102/records" 2>&1)
  local UNRESOLVED
  UNRESOLVED=$(echo "$SYSPRINT" | grep '^IEW0132  [A-Z@#$]' | awk '{print $2}' | sort -u || true)
  local UCOUNT=0
  if [ -n "$UNRESOLVED" ]; then
    UCOUNT=$(echo "$UNRESOLVED" | wc -l | tr -d ' ')
  fi

  if [ "$UCOUNT" -gt 0 ]; then
    printf "  %-10s %-12s %s  unresolved: %s\n" "$MOD" "$JOBID" "$RETCODE" "$(echo $UNRESOLVED | tr '\n' ' ')"
  else
    printf "  %-10s %-12s %s\n" "$MOD" "$JOBID" "$RETCODE"
  fi
}

echo "=== Linking httpd load modules ==="
echo ""
for MOD in $MODULES; do
  link_module "$MOD"
done
echo ""
echo "=== Done ==="

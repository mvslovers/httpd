//HTTPDACT JOB (ACCT),'ACTIVATE HTTPD',CLASS=A,MSGCLASS=H,
//         NOTIFY=&SYSUID
//*
//* Compress the httpd STEPLIB and copy freshly deployed HTTPD load
//* modules into it.
//*
//* WHY THIS EXISTS
//*   `make deploy` RECEIVEs into the deploy library built from
//*   MBT_MVS_HLQ in .env -- by default
//*   <hlq>.HTTPD.V4R0M0D.LINKLIB -- and stops there. The started
//*   task runs from a different data set (its STEPLIB DD), so a
//*   deploy on its own changes nothing that is running.
//*
//*   Unlike the MVSMF case there is no hot activation here. httpd
//*   loads a CGI fresh per request, so copying MVSMF into the
//*   STEPLIB activates it immediately; HTTPD is the running
//*   program itself and only picks up a new module on S HTTPD.
//*
//* BEFORE YOU SUBMIT
//*   Both steps use DISP=OLD, because IEBCOPY COMPRESS cannot run
//*   against a library httpd holds SHR through its STEPLIB. The
//*   job therefore WAITS in the enqueue until httpd releases it.
//*   Submit this first, then stop httpd -- it starts the moment
//*   the STC ends. Note that a waiting job occupies an initiator.
//*
//*     <submit this job>
//*     P HTTPD          (3270 / console)
//*     <job runs>
//*     S HTTPD
//*
//*   P HTTPD may not be enough if workers are wedged: the address
//*   space can stay up with `cthread_manager_term(...): dispatch
//*   thread did not stop`. Follow with C HTTPD. Afterwards TCPIP
//*   holds port 8080 for a while and httpd does NOT wait it out --
//*   it logs a few HTTPD030I EADDRINUSE, then HTTPD404E, and ends
//*   itself. Retry S HTTPD until HTTPD001I Server is READY.
//*
//* CHECK THE NAMES
//*   Both data sets below are installation-specific. The STEPLIB
//*   name differs per stand -- confirm it against the PROC rather
//*   than trusting this file:
//*
//*     SYS2.PROCLIB(HTTPD)  ->  //STEPLIB DD DSN=...
//*
//*   The deploy library is the one `make deploy` prints as its
//*   target.
//*
//* AFTERWARDS
//*   Confirm the new module is really the one running -- a deploy
//*   to the wrong library fails silently. /.dsrv?target=MGR shows
//*   mgr->func; the address must change when the build changes.
//*   IEBCOPY also prints IEB144I with the tracks left, which is
//*   the early warning for the next SE37.
//*
//COMPRESS EXEC PGM=IEBCOPY
//SYSPRINT DD SYSOUT=*
//LIB      DD DSN=HTTPD.LINKLIB,DISP=OLD
//SYSIN    DD *
  COPY INDD=LIB,OUTDD=LIB
/*
//ACTIVATE EXEC PGM=IEBCOPY
//SYSPRINT DD SYSOUT=*
//IN       DD DSN=IBMUSER.HTTPD.V4R0M0D.LINKLIB,DISP=SHR
//OUT      DD DSN=HTTPD.LINKLIB,DISP=OLD
//SYSIN    DD *
  COPY INDD=((IN,R)),OUTDD=OUT
  SELECT MEMBER=HTTPD
  SELECT MEMBER=HTTPDM
  SELECT MEMBER=HTTPDMTT
  SELECT MEMBER=HTTPDSRV
  SELECT MEMBER=ABEND0C1
/*
//*
//* ABEND0C1 allocates and then abends S0C1 on purpose -- the probe for
//* the per-route storage reclaim (issue #154).  Copying it in is
//* harmless: like every module since 4.0.0 it does nothing at all
//* unless a Parmlib route names it.

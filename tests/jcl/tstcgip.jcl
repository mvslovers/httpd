//TSTCGIP  JOB (A),'HTTPD CGIP',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* #275 - a parm length of 1..3 with a zero third byte is not
//* TSO-shaped in cgistart either.  See test/mvs/tstcgip.c.  TSTCGIP
//* LINKs the target named in PARM with R1 -> { A(X'0002',X'00','X'),
//* A(HTTPD), A(HTTPC)|VL }.  TSTCGPT is linked with the fixed
//* cgistart, TSTCGPR with the one before it - the red control.
//*
//* Run:     mvsdev JOB01434, 2026-10-05: GREEN CC 0, RED CC 1.
//*
//* RC 0 = the target ran and returned 7, 1 = it did not.
//*
//* The library is a scratch one, RECEIVEd from the three modules as
//* tstcgip.c describes; the HTTPD* DDs are the ones cgistart opens.
//*
//GREEN    EXEC PGM=TSTCGIP,REGION=4M,PARM='TSTCGPT'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.HTTPD.P275.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//HTTPDOUT DD  SYSOUT=*
//HTTPDERR DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTCGIP,REGION=4M,PARM='TSTCGPR',COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.HTTPD.P275.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//HTTPDOUT DD  SYSOUT=*
//HTTPDERR DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*

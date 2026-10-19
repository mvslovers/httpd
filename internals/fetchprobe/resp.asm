*---------------------------------------------------------------------
* RESP - DOES LINK REUSE A COPY THAT IS HELD BY LOAD? (HTTPD#250)
*
* THE TEST MODULES RPRBA..RPRBE ARE IN DD XLIB, WHICH IS ALSO IN
* STEPLIB.  AFTER A MODULE IS LOADED, ITS MEMBER IS DELETED FROM
* XLIB (STOW D).  A LINK THAT STILL WORKS THEN USED THE COPY IN
* STORAGE; A LINK THAT FAILS 806-4 WENT TO THE LIBRARY.
*   A  NO LOAD: LINK, DELETE MEMBER, LINK      (EXPECT OK, 806)
*   B  LOAD EP=, DELETE MEMBER, LINK; DELETE MODULE, LINK
*   C  AS B, BUT THE MODULE IS NOT REUSABLE
*   D  LOAD EP= HERE, DELETE MEMBER, LINK IN A SUBTASK
*   E  BLDL + LOAD DE= (THE LIBC370 __LOAD PATH), DELETE MEMBER,
*      LINK HERE AND IN A SUBTASK
* EVERY RESULT IS A WTO (ROUTCDE 11).  RC 0 ALWAYS.
*---------------------------------------------------------------------
RESP     CSECT
         SAVE  (14,12),,RESP
         LR    12,15
         USING RESP,12
         LA    11,SAVEA
         ST    13,4(,11)
         ST    11,8(,13)
         LR    13,11
*
*        A: NO LOAD
*
         MVC   CASE,=C'A1'
         LA    2,NAMEA
         BAL   14,LNK
         BAL   14,DELM
         MVC   CASE,=C'A2'
         LA    2,NAMEA
         BAL   14,LNK
*
*        B: LOAD EP=, REUS
*
         MVC   CASE,=C'B0'
         LOAD  EPLOC=NAMEB,ERRET=LDERR
         BAL   14,LDOK
         LA    2,NAMEB
         BAL   14,DELM
         MVC   CASE,=C'B1'
         LA    2,NAMEB
         BAL   14,LNK
         DELETE EPLOC=NAMEB
         MVC   CASE,=C'B2'
         LA    2,NAMEB
         BAL   14,LNK
*
*        C: LOAD EP=, NOT REUSABLE
*
         MVC   CASE,=C'C0'
         LOAD  EPLOC=NAMEC,ERRET=LDERR
         BAL   14,LDOK
         LA    2,NAMEC
         BAL   14,DELM
         MVC   CASE,=C'C1'
         LA    2,NAMEC
         BAL   14,LNK
         DELETE EPLOC=NAMEC
*
*        D: LOAD EP= HERE, LINK IN A SUBTASK
*
         MVC   CASE,=C'D0'
         LOAD  EPLOC=NAMED,ERRET=LDERR
         BAL   14,LDOK
         LA    2,NAMED
         BAL   14,DELM
         MVC   CASE,=C'D1'
         LA    2,NAMED
         BAL   14,SUBT
         DELETE EPLOC=NAMED
*
*        E: BLDL + LOAD DE=, DCB=0, AS LIBC370 __LOAD DOES
*
         MVC   CASE,=C'E0'
         BLDL  0,BLDLE
         ST    15,BLDLRC
         MVC   MSG,BLANKS
         MVC   MSG(4),=C'RESP'
         MVC   MSG+5(2),CASE
         MVC   MSG+8(10),=C'BLDL RPRBE'
         MVC   MSG+20(3),=C'RC='
         L     0,BLDLRC
         LA    1,MSG+23
         BAL   14,HEX8
         MVC   MSG+32(2),=C'Z='
         SR    0,0
         IC    0,BLDLEZ
         LA    1,MSG+34
         BAL   14,HEX8
         BAL   14,SAY
         LA    2,BLDLEN
         SR    3,3
         LOAD  DE=(2),DCB=(3),ERRET=LDERR
         BAL   14,LDOK
         LA    2,NAMEE
         BAL   14,DELM
         MVC   CASE,=C'E1'
         LA    2,NAMEE
         BAL   14,LNK
         MVC   CASE,=C'E2'
         LA    2,NAMEE
         BAL   14,SUBT
         DELETE EPLOC=NAMEE
*
         MVC   MSG,BLANKS
         MVC   MSG(8),=C'RESP END'
         BAL   14,SAY
         L     13,4(,13)
         RETURN (14,12),RC=0
*
*        LINK TO THE MODULE NAMED AT 0(R2), WITH AN ERROR EXIT
*
LNK      ST    14,LNKR14
         MVC   LPLNAME,0(2)
         MVC   MSG,BLANKS
         MVC   MSG(4),=C'RESP'
         MVC   MSG+5(2),CASE
         MVC   MSG+8(4),=C'LINK'
         MVC   MSG+13(8),0(2)
         LA    15,LPL
         SR    1,1
         SVC   6                  LINK
         ST    15,ERRR15
         MVC   MSG+22(2),=C'OK'
         MVC   MSG+25(3),=C'RC='
         L     0,ERRR15
         LA    1,MSG+28
         BAL   14,HEX8
         B     LNKSAY
LNKERR   ST    1,ERRR1            ERROR EXIT: R1 CODE, R15 REASON
         ST    15,ERRR15
         MVC   MSG+22(4),=C'FAIL'
         MVC   MSG+27(3),=C'R1='
         L     0,ERRR1
         LA    1,MSG+30
         BAL   14,HEX8
         MVC   MSG+39(4),=C'R15='
         L     0,ERRR15
         LA    1,MSG+43
         BAL   14,HEX8
LNKSAY   BAL   14,SAY
         L     14,LNKR14
         BR    14
*
*        ATTACH RPRBS, WHICH LINKS TO THE MODULE NAMED AT 0(R2)
*
SUBT     ST    14,SUBR14
         ST    2,SUBPARM
         OI    SUBPARM,X'80'
         XC    ECBS,ECBS
         LA    1,SUBPARM
         ATTACH EP=RPRBS,ECB=ECBS
         ST    1,SUBTCB
         WAIT  ECB=ECBS
         DETACH SUBTCB
         MVC   MSG,BLANKS
         MVC   MSG(4),=C'RESP'
         MVC   MSG+5(2),CASE
         MVC   MSG+8(12),=C'SUBTASK ECB='
         L     0,ECBS
         LA    1,MSG+20
         BAL   14,HEX8
         BAL   14,SAY
         L     14,SUBR14
         BR    14
*
*        DELETE THE MEMBER NAMED AT 0(R2) FROM XLIB, THEN BLDL IT
*
DELM     ST    14,DELR14
         MVC   STOWN,0(2)
         OPEN  (XDCB,OUTPUT)
         STOW  XDCB,STOWN,D
         ST    15,STOWRC
         CLOSE (XDCB)
         MVC   BLDLN(8),0(2)
         BLDL  0,BLDLL
         ST    15,BLDLRC
         MVC   MSG,BLANKS
         MVC   MSG(4),=C'RESP'
         MVC   MSG+5(2),CASE
         MVC   MSG+8(4),=C'STOW'
         MVC   MSG+13(8),0(2)
         MVC   MSG+22(3),=C'RC='
         L     0,STOWRC
         LA    1,MSG+25
         BAL   14,HEX8
         MVC   MSG+34(8),=C'BLDL RC='
         L     0,BLDLRC
         LA    1,MSG+42
         BAL   14,HEX8
         BAL   14,SAY
         L     14,DELR14
         BR    14
*
*        LOAD RESULTS
*
LDOK     ST    14,LDR14
         ST    0,ERRR1            EP ADDRESS
         MVC   MSG,BLANKS
         MVC   MSG(4),=C'RESP'
         MVC   MSG+5(2),CASE
         MVC   MSG+8(7),=C'LOAD OK'
         MVC   MSG+16(3),=C'EP='
         L     0,ERRR1
         LA    1,MSG+19
         BAL   14,HEX8
         BAL   14,SAY
         L     14,LDR14
         BR    14
LDERR    ST    1,ERRR1
         ST    15,ERRR15
         MVC   MSG,BLANKS
         MVC   MSG(4),=C'RESP'
         MVC   MSG+5(2),CASE
         MVC   MSG+8(9),=C'LOAD FAIL'
         MVC   MSG+18(3),=C'R1='
         L     0,ERRR1
         LA    1,MSG+21
         BAL   14,HEX8
         BAL   14,SAY
         L     13,4(,13)
         RETURN (14,12),RC=8
*
SAY      ST    14,SAYR14          WTO THE 80-BYTE MSG, ROUTCDE 11
         LA    1,WTOL
         SVC   35
         L     14,SAYR14
         BR    14
HEX8     ST    0,HEXW             R0 AS 8 HEX DIGITS AT 0(R1)
         UNPK  HEXO(9),HEXW(5)
         TR    HEXO(8),HEXTAB-240
         MVC   0(8,1),HEXO
         BR    14
*
         LTORG
SAVEA    DS    18F
LNKR14   DC    F'0'
SUBR14   DC    F'0'
DELR14   DC    F'0'
LDR14    DC    F'0'
SAYR14   DC    F'0'
ERRR1    DC    F'0'
ERRR15   DC    F'0'
STOWRC   DC    F'0'
BLDLRC   DC    F'0'
SUBPARM  DC    F'0'
SUBTCB   DC    F'0'
ECBS     DC    F'0'
HEXW     DC    F'0'
         DC    X'00'
HEXO     DC    CL9' '
CASE     DC    CL2' '
NAMEA    DC    CL8'RPRBA'
NAMEB    DC    CL8'RPRBB'
NAMEC    DC    CL8'RPRBC'
NAMED    DC    CL8'RPRBD'
NAMEE    DC    CL8'RPRBE'
STOWN    DC    CL8' '
         DS    0F
LPL      DC    A(LPLNAME)         LINK SVC PARAMETER LIST
         DC    X'80',AL3(0)       NO DCB, ERROR EXIT FOLLOWS
         DC    A(LNKERR)
LPLNAME  DC    CL8' '
         DS    0H
BLDLL    DC    H'1',H'76'         BLDL LIST FOR THE CHECK AFTER STOW
BLDLN    DC    CL8' ',XL68'00'
         DS    0H
BLDLE    DC    H'1',H'76'         BLDL LIST FOR CASE E
BLDLEN   DC    CL8'RPRBE',XL4'00'
BLDLEZ   DC    X'00'
         DC    XL63'00'
         DS    0F
WTOL     DC    AL2(4+80),X'8000'  MCS FLAGS: ROUTING/DESCRIPTOR CODES
MSG      DC    CL80' '
         DC    X'0000'            DESCRIPTOR CODES
         DC    X'0020'            ROUTING CODE 11
BLANKS   DC    CL80' '
HEXTAB   DC    C'0123456789ABCDEF'
XDCB     DCB   DDNAME=XLIB,DSORG=PO,MACRF=W
         END   RESP

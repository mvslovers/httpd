*---------------------------------------------------------------------
* LDLP - PROGRAM FETCH PROBE
*
* PARM='NAME,COUNT'.  LOADS MODULE NAME COUNT TIMES, EACH LOAD
* FOLLOWED BY A DELETE, SO EVERY LOAD IS A FRESH PROGRAM FETCH.
*   - A FAILED FETCH TAKES THE ERRET EXIT: COUNTED AS FAIL, FIRST 10
*     REPORTED WITH THE R1 AND R15 THE EXIT RECEIVED.
*   - A FETCH THAT SUCCEEDS IS CHECKSUMMED OVER THE WHOLE MAIN
*     STORAGE BLOCK OF THE MODULE (CDE EXTENT LIST).  A DIFFERENT
*     SUM AT THE SAME LOAD ADDRESS AS THE FIRST LOAD IS COUNTED AS
*     BAD (SILENTLY WRONG CONTENTS); A DIFFERENT ADDRESS AS MOVED
*     (RELOCATED ADCONS CHANGE THE SUM); THE NEW ADDRESS BECOMES THE
*     BASELINE THE NEXT LOADS ARE COMPARED WITH.
* MESSAGES GO TO THE JOB LOG (WTO ROUTCDE=11).
* RC 0 CLEAN, 4 MOVED/NOCDE ONLY, 8 FAIL OR BAD, 16 PARM ERROR.
*---------------------------------------------------------------------
LDLP     CSECT
         SAVE  (14,12),,LDLP
         LR    12,15
         USING LDLP,12
         LA    11,SAVEA
         ST    13,4(,11)
         ST    11,8(,13)
         LR    13,11
*
*        PARSE PARM='NAME,COUNT'
*
         L     1,0(,1)            -> HALFWORD LENGTH + TEXT
         LH    2,0(,1)            PARM LENGTH
         LA    3,2(,1)            -> PARM TEXT
         LA    4,MODNAME
         LA    5,8                MAX NAME LENGTH
PNAME    LTR   2,2
         BZ    PERR
         CLI   0(3),C','
         BE    PNUM
         LTR   5,5
         BZ    PERR               NAME LONGER THAN 8
         MVC   0(1,4),0(3)
         LA    4,1(,4)
         LA    3,1(,3)
         BCTR  2,0
         BCTR  5,0
         B     PNAME
PNUM     CLI   MODNAME,C' '
         BE    PERR               EMPTY NAME
         LA    3,1(,3)            SKIP THE COMMA
         BCTR  2,0
         LTR   2,2
         BNP   PERR               NO COUNT
         CH    2,=H'9'
         BH    PERR               MORE THAN 9 DIGITS
         LR    4,3                CHECK DIGITS ONLY
         LR    5,2
PDIG     CLI   0(4),C'0'
         BL    PERR
         CLI   0(4),C'9'
         BH    PERR
         LA    4,1(,4)
         BCT   5,PDIG
         BCTR  2,0                LENGTH-1 FOR EX
         EX    2,PACKNUM
         CVB   6,DWORD
         LTR   6,6
         BNP   PERR
         ST    6,ITERS
*
*        MAIN LOOP
*
         SR    7,7                ITERATION NUMBER
LOOP     LA    7,1(,7)
         ST    7,ITER
         LOAD  EPLOC=MODNAME,ERRET=LOADERR
*                                 FIND THE CDE ON OUR JOB PACK QUEUE
         L     1,X'21C'           PSATOLD -> OUR TCB
         L     1,X'2C'(,1)        TCBJPQ
         LA    1,0(,1)            DROP THE PURGE FLAG BYTE
CDELOOP  LTR   1,1
         BZ    NOCDE
         CLC   8(8,1),MODNAME     CDNAME
         BE    GOTCDE
         L     1,0(,1)            CDCHAIN
         LA    1,0(,1)
         B     CDELOOP
GOTCDE   TM    X'1C'(1),X'04'     CDATTR CDMIN: MINOR (ALIAS) CDE?
         BZ    MAJOR
         L     1,X'14'(,1)        -> MAJOR CDE
         LA    1,0(,1)
MAJOR    TM    X'1D'(1),X'20'     CDATTR2 CDXLE: EXTENT LIST BUILT?
         BZ    NOCDE
         L     2,X'14'(,1)        CDXLMJP -> EXTENT LIST
         LA    2,0(,2)
         L     3,X'0C'(,2)        XTLMSBAD
         LA    3,0(,3)
         L     4,X'08'(,2)        XTLMSBLN
         LA    4,0(,4)            DROP THE X'80'
         LR    5,4
         SRL   5,2                FULLWORDS
         LTR   5,5
         BZ    NOCDE
         SR    8,8                SUM1
         SR    9,9                SUM2 (POSITION SENSITIVE)
         LR    6,3
CKLOOP   AL    8,0(,6)
         ALR   9,8
         LA    6,4(,6)
         BCT   5,CKLOOP
         CLI   HAVE1,1
         BE    COMPARE
*                                 FIRST GOOD LOAD: REMEMBER IT
         ST    3,ADDR1
         ST    4,LEN1
         ST    8,SUM1A
         ST    9,SUM2A
         MVI   HAVE1,1
         LR    0,4                KEEP A COPY OF THE GOOD IMAGE
         GETMAIN R,LV=(0)
         ST    1,COPYA
         ST    4,COPYL
         LR    14,1
         LR    15,4
         LR    0,3
         LR    1,4
         MVCL  14,0
         MVC   MSG,BLANKS
         MVC   MSG(8),=C'LDLP 1ST'
         MVC   MSG+9(8),MODNAME
         MVC   MSG+18(3),=C'AT='
         LR    0,3
         LA    1,MSG+21
         BAL   14,HEX8
         MVC   MSG+30(4),=C'LEN='
         LR    0,4
         LA    1,MSG+34
         BAL   14,HEX8
         MVC   MSG+43(3),=C'S1='
         LR    0,8
         LA    1,MSG+46
         BAL   14,HEX8
         MVC   MSG+55(3),=C'S2='
         LR    0,9
         LA    1,MSG+58
         BAL   14,HEX8
         BAL   14,SAY
         B     DODEL
COMPARE  C     3,ADDR1
         BNE   ISMOVED
         C     4,LEN1
         BNE   ISBAD
         C     8,SUM1A
         BNE   ISBAD
         C     9,SUM2A
         BNE   ISBAD
         B     DODEL
ISMOVED  L     15,MOVED
         LA    15,1(,15)
         ST    15,MOVED
         ST    3,ADDR1            NEW BASELINE AT THE NEW ADDRESS
         ST    4,LEN1
         ST    8,SUM1A
         ST    9,SUM2A
         C     4,COPYL            SAME LENGTH: NEW COPY, ELSE NO DIFFS
         BE    MVCOPY
         XC    COPYA,COPYA
         B     MVMSG
MVCOPY   L     14,COPYA
         LR    15,4
         LR    0,3
         LR    1,4
         MVCL  14,0
MVMSG    L     15,MOVED
         C     15,=F'10'
         BH    DODEL
         MVC   MSG,BLANKS
         MVC   MSG(10),=C'LDLP MOVED'
         MVC   MSG+11(2),=C'I='
         L     0,ITER
         LA    1,MSG+13
         BAL   14,DEC7
         MVC   MSG+21(3),=C'AT='
         LR    0,3
         LA    1,MSG+24
         BAL   14,HEX8
         BAL   14,SAY
         B     DODEL
ISBAD    L     15,BADS
         LA    15,1(,15)
         ST    15,BADS
         C     15,=F'10'
         BH    DODEL
         MVC   MSG,BLANKS
         MVC   MSG(8),=C'LDLP BAD'
         MVC   MSG+11(2),=C'I='
         L     0,ITER
         LA    1,MSG+13
         BAL   14,DEC7
         MVC   MSG+21(3),=C'AT='
         LR    0,3
         LA    1,MSG+24
         BAL   14,HEX8
         MVC   MSG+33(4),=C'LEN='
         LR    0,4
         LA    1,MSG+37
         BAL   14,HEX8
         MVC   MSG+46(3),=C'S1='
         LR    0,8
         LA    1,MSG+49
         BAL   14,HEX8
         MVC   MSG+58(3),=C'S2='
         LR    0,9
         LA    1,MSG+61
         BAL   14,HEX8
         BAL   14,SAY
         L     10,COPYA           WHERE DOES IT DIFFER FROM THE COPY?
         LTR   10,10
         BZ    DODEL
         C     4,COPYL
         BNE   DODEL
         LR    6,3
         LR    5,4
         SRL   5,2
         SR    11,11              DIFFERING WORDS
         L     2,=F'-1'           FIRST DIFFERING OFFSET
         SR    7,7                LAST DIFFERING OFFSET
DIFFLP   CLC   0(4,6),0(10)
         BE    DIFFNX
         LA    11,1(,11)
         LR    15,6
         SR    15,3
         LR    7,15
         LTR   2,2
         BNM   DIFFNX
         LR    2,15
DIFFNX   LA    6,4(,6)
         LA    10,4(,10)
         BCT   5,DIFFLP
         MVC   MSG,BLANKS
         MVC   MSG(9),=C'LDLP DIFF'
         MVC   MSG+11(6),=C'WORDS='
         LR    0,11
         LA    1,MSG+17
         BAL   14,HEX8
         MVC   MSG+26(6),=C'FIRST='
         LR    0,2
         LA    1,MSG+32
         BAL   14,HEX8
         MVC   MSG+41(5),=C'LAST='
         LR    0,7
         LA    1,MSG+46
         BAL   14,HEX8
         BAL   14,SAY
         LTR   2,2                SHOW 16 BYTES AT THE FIRST DIFFERENCE
         BM    DODEL
         LA    6,0(2,3)           LOADED
         L     10,COPYA
         LA    10,0(2,10)         GOOD COPY
         MVC   MSG,BLANKS
         MVC   MSG(9),=C'LDLP GOOD'
         LA    5,4
         LR    8,10
         LA    9,MSG+11
         BAL   14,HEXN
         BAL   14,SAY
         MVC   MSG,BLANKS
         MVC   MSG(9),=C'LDLP BAD '
         LA    5,4
         LR    8,6
         LA    9,MSG+11
         BAL   14,HEXN
         BAL   14,SAY
         B     DODEL
NOCDE    L     15,NOCDES
         LA    15,1(,15)
         ST    15,NOCDES
DODEL    DELETE EPLOC=MODNAME
NEXT     L     7,ITER
         C     7,ITERS
         BL    LOOP
         B     SUMMARY
*
*        ERRET EXIT OF THE LOAD: R1 AND R15 AS RECEIVED
*
LOADERR  ST    1,ERRR1
         ST    15,ERRR15
         L     15,FAILS
         LA    15,1(,15)
         ST    15,FAILS
         C     15,=F'10'
         BH    NEXT
         MVC   MSG,BLANKS
         MVC   MSG(9),=C'LDLP FAIL'
         MVC   MSG+11(2),=C'I='
         L     0,ITER
         LA    1,MSG+13
         BAL   14,DEC7
         MVC   MSG+21(3),=C'R1='
         L     0,ERRR1
         LA    1,MSG+24
         BAL   14,HEX8
         MVC   MSG+33(4),=C'R15='
         L     0,ERRR15
         LA    1,MSG+37
         BAL   14,HEX8
         BAL   14,SAY
         B     NEXT
*
*        SUMMARY AND RETURN CODE
*
SUMMARY  MVC   MSG,BLANKS
         MVC   MSG(4),=C'LDLP'
         MVC   MSG+5(8),MODNAME
         MVC   MSG+14(2),=C'N='
         L     0,ITERS
         LA    1,MSG+16
         BAL   14,DEC7
         MVC   MSG+24(5),=C'FAIL='
         L     0,FAILS
         LA    1,MSG+29
         BAL   14,DEC7
         MVC   MSG+37(4),=C'BAD='
         L     0,BADS
         LA    1,MSG+41
         BAL   14,DEC7
         MVC   MSG+49(6),=C'MOVED='
         L     0,MOVED
         LA    1,MSG+55
         BAL   14,DEC7
         MVC   MSG+63(6),=C'NOCDE='
         L     0,NOCDES
         LA    1,MSG+69
         BAL   14,DEC7
         BAL   14,SAY
         LA    15,8
         L     0,FAILS
         A     0,BADS
         LTR   0,0
         BNZ   EXIT
         LA    15,4
         L     0,MOVED
         A     0,NOCDES
         LTR   0,0
         BNZ   EXIT
         SR    15,15
         B     EXIT
PERR     MVC   MSG,BLANKS
         MVC   MSG(36),=C'LDLP PARM ERROR, USE PARM=NAME,COUNT'
         BAL   14,SAY
         LA    15,16
EXIT     L     13,4(,13)
         RETURN (14,12),RC=(15)
*
*        SUBROUTINES (CALLED BY BAL 14)
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
HEXN     ST    14,HEXNR14         R5 WORDS AT 0(R8) AS HEX AT 0(R9)
HEXNLP   L     0,0(,8)
         LR    1,9
         BAL   14,HEX8
         LA    8,4(,8)
         LA    9,9(,9)
         BCT   5,HEXNLP
         L     14,HEXNR14
         BR    14
DEC7     CVD   0,DWORD            R0 AS 7 DECIMAL DIGITS AT 0(R1)
         UNPK  0(7,1),DWORD(8)
         OI    6(1),X'F0'
         BR    14
PACKNUM  PACK  DWORD(8),0(0,3)    EXECUTED
*
         LTORG
SAVEA    DS    18F
DWORD    DS    D
ITERS    DC    F'0'
ITER     DC    F'0'
FAILS    DC    F'0'
BADS     DC    F'0'
MOVED    DC    F'0'
NOCDES   DC    F'0'
ADDR1    DC    F'0'
LEN1     DC    F'0'
SUM1A    DC    F'0'
SUM2A    DC    F'0'
ERRR1    DC    F'0'
ERRR15   DC    F'0'
SAYR14   DC    F'0'
HEXNR14  DC    F'0'
COPYA    DC    F'0'
COPYL    DC    F'0'
HEXW     DC    F'0'
         DC    X'00'
HEXO     DC    CL9' '
HAVE1    DC    X'00'
MODNAME  DC    CL8' '
         DS    0F
WTOL     DC    AL2(4+80),X'8000'  MCS FLAGS: ROUTING/DESCRIPTOR CODES
MSG      DC    CL80' '
         DC    X'0000'            DESCRIPTOR CODES
         DC    X'0020'            ROUTING CODE 11
BLANKS   DC    CL80' '
HEXTAB   DC    C'0123456789ABCDEF'
         END   LDLP

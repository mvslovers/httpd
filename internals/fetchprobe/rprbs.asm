*---------------------------------------------------------------------
* RPRBS - SUBTASK OF RESP: LINK TO THE MODULE WHOSE NAME THE PARM
* WORD POINTS AT, WITH AN ERROR EXIT, AND SAY WHAT HAPPENED.
*---------------------------------------------------------------------
RPRBS    CSECT
         SAVE  (14,12),,RPRBS
         LR    12,15
         USING RPRBS,12
         LA    11,SAVEA
         ST    13,4(,11)
         ST    11,8(,13)
         LR    13,11
         L     2,0(,1)
         LA    2,0(,2)
         MVC   LPLNAME,0(2)
         MVC   MSG,BLANKS
         MVC   MSG(10),=C'RPRBS LINK'
         MVC   MSG+11(8),0(2)
         LA    15,LPL
         SR    1,1
         SVC   6                  LINK
         ST    15,ERRR15
         MVC   MSG+20(2),=C'OK'
         MVC   MSG+23(3),=C'RC='
         L     0,ERRR15
         LA    1,MSG+26
         BAL   14,HEX8
         LA    1,WTOL
         SVC   35
         L     13,4(,13)
         RETURN (14,12),RC=0
LNKERR   ST    1,ERRR1
         ST    15,ERRR15
         MVC   MSG+20(4),=C'FAIL'
         MVC   MSG+25(3),=C'R1='
         L     0,ERRR1
         LA    1,MSG+28
         BAL   14,HEX8
         MVC   MSG+37(4),=C'R15='
         L     0,ERRR15
         LA    1,MSG+41
         BAL   14,HEX8
         LA    1,WTOL
         SVC   35
         L     13,4(,13)
         RETURN (14,12),RC=8
HEX8     ST    0,HEXW
         UNPK  HEXO(9),HEXW(5)
         TR    HEXO(8),HEXTAB-240
         MVC   0(8,1),HEXO
         BR    14
         LTORG
SAVEA    DS    18F
ERRR1    DC    F'0'
ERRR15   DC    F'0'
HEXW     DC    F'0'
         DC    X'00'
HEXO     DC    CL9' '
         DS    0F
LPL      DC    A(LPLNAME)
         DC    X'80',AL3(0)
         DC    A(LNKERR)
LPLNAME  DC    CL8' '
         DS    0F
WTOL     DC    AL2(4+80),X'8000'
MSG      DC    CL80' '
         DC    X'0000'
         DC    X'0020'
BLANKS   DC    CL80' '
HEXTAB   DC    C'0123456789ABCDEF'
         END   RPRBS

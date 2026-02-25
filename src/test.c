

__asm__("\n"
"SUBTASK\tDS\t0D\n"
"         SAVE  (14,12),,'SUBTASK'\n"
"         LA    12,0(,15)\n"
"         USING SUBTASK,12\n"
"         LA    11,0(,1)\n"
"         L     0,4(,11)          get stack size\n"
"         C     0,=A(STACKMIN)    is stack large enough\n"
"         BH    GETMAIN           yes, continue\n"
"         L     0,=A(STACKLEN)    no, use default size\n"
"GETMAIN  DS    0H\n"
"         GETMAIN RU,LV=(0),SP=0\n"
"         ST    13,4(,1)\n"
"         ST    1,8(,13)\n"
"         LR    13,1\n"
"         USING STACK,13\n"
"         LA    0,MAINSTK         stack for called functions\n"
"         ST    0,SAVENAB         next available byte in stack\n"
"*");

__asm__("\n"
"         L     15,0(,11)         get function address from plist\n"
"         LA    1,4(11)           => parameter for function\n"
"         BALR  14,15             call function\n"
"         LR    10,15             save return code from function\n"
"*");

__asm__("\n"
"         LR    1,13\n"
"         L     13,SAVEAREA+4\n"
"         L     0,4(,11)          get stack size\n"
"         C     0,=A(STACKMIN)    is stack large enough\n"
"         BH    FREEMAIN          yes, continue\n"
"         L     0,=A(STACKLEN)    no, use default size\n"
"FREEMAIN DS    0H\n"
"         FREEMAIN RU,LV=(0),A=(1),SP=0\n"
"         LR    15,10\n"
"         RETURN (14,12),RC=(15)\n"
"*");

__asm__("LTORG ,");

__asm__("\n"
"STACK    DSECT\n"
"SAVEAREA DS    18F               00 (0)  callers registers go here\n"
"SAVELWS  DS    A                 48 (72) PL/I Language Work Space N/A\n"
"SAVENAB  DS    A                 4C (76) next available byte -------+\n"
"* start of C function stack area                                    |\n"
"         DS    0D                                                   |\n"
"STACKMIN EQU   *-STACK                                              |\n"
"MAINSTK  DS    32768F            128K bytes <-----------------------+\n"
"MAINLEN  EQU   *-MAINSTK\n"
"STACKLEN EQU   *-STACK");

__asm__("CSECT ,");

int main(int argc, char**argv)
{
    void    *func = 0;
    int     stack = 65536;

    __asm__("L\t0,=A(SUBTASK)\n\tST\t0,%0" : "=m"(func));

    printf("func=%08X, stack=%d\n", func, stack);

    return 0;
}

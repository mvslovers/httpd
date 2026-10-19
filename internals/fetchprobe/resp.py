# usage: resp.py HLQ > resp.jcl
# Writes the LINK reuse probe job (#250) to stdout: assembles RESP and RPRBS
# (resp.asm, rprbs.asm) and the trivial test module, links RPRBA..RPRBE into
# HLQ.RESPPRB.XLIB (RPRBC without REUS), runs RESP with that library in
# STEPLIB, and deletes the library again.  Submit it with subjcl.py.
import os, sys
hlq = sys.argv[1].upper()
xlib = hlq + ".RESPPRB.XLIB"
here = os.path.dirname(os.path.abspath(__file__))
src = lambda f: open(os.path.join(here, f)).read()

def asm(n, text):
    return f"""//A{n:<7} EXEC PGM=IFOX00,PARM='OBJECT,NODECK,NOXREF'
//SYSLIB   DD DSN=SYS1.MACLIB,DISP=SHR
//SYSUT1   DD UNIT=SYSDA,SPACE=(CYL,(2,1))
//SYSUT2   DD UNIT=SYSDA,SPACE=(CYL,(2,1))
//SYSUT3   DD UNIT=SYSDA,SPACE=(CYL,(2,1))
//SYSPRINT DD SYSOUT=*
//SYSPUNCH DD DUMMY
//SYSGO    DD DSN=&&O{n},UNIT=SYSDA,SPACE=(TRK,(5,5)),DISP=(NEW,PASS)
//SYSIN    DD *
{text}/*
"""

def lked(step, parm, lin, lmod, disp):
    return f"""//{step:<8} EXEC PGM=IEWL,PARM='{parm}'
//SYSPRINT DD SYSOUT=*
//SYSUT1   DD UNIT=SYSDA,SPACE=(CYL,(1,1))
//OBJ      DD DSN=&&OM,DISP=(OLD,PASS)
//OBR      DD DSN=&&OR,DISP=(OLD,PASS)
//OBS      DD DSN=&&OS,DISP=(OLD,PASS)
//SYSLMOD  DD DSN={lmod},UNIT=SYSDA,SPACE=(TRK,(5,5,5)),
//            DISP={disp}
//SYSLIN   DD *
{lin}/*
"""

rprb = """RPRB     CSECT
         SR    15,15
         BR    14
         END   RPRB
"""
j = ("//RESPPRB JOB (RESP),'LINK REUSE PROBE',CLASS=A,MSGCLASS=H,\n"
     "//             REGION=4M\n"
     "//* DOES LINK REUSE A COPY HELD BY LOAD? (HTTPD#250, SEE RESP SOURCE)\n"
     f"//CLEAN0   EXEC PGM=IEFBR14\n//X        DD DSN={xlib},DISP=(MOD,DELETE),UNIT=SYSDA,\n"
     "//            SPACE=(TRK,(1,1))\n")
j += asm("R", src("resp.asm")) + asm("S", src("rprbs.asm")) + asm("M", rprb)
j += lked("LKPRB", "LIST,MAP", " INCLUDE OBR\n NAME RESP(R)\n INCLUDE OBS\n NAME RPRBS(R)\n",
          "&&LOAD", "(NEW,PASS)")
j += lked("LKREUS", "LIST,RENT,REUS", "".join(f" INCLUDE OBJ\n NAME RPRB{x}(R)\n" for x in "ABDE"),
          xlib, "(NEW,CATLG,DELETE)")
j += lked("LKNORE", "LIST", " INCLUDE OBJ\n NAME RPRBC(R)\n", xlib, "OLD")
j += f"""//GO       EXEC PGM=RESP
//STEPLIB  DD DSN=&&LOAD,DISP=(OLD,PASS)
//         DD DSN={xlib},DISP=SHR
//XLIB     DD DSN={xlib},DISP=OLD
//CLEAN    EXEC PGM=IEFBR14,COND=EVEN
//X        DD DSN={xlib},DISP=(OLD,DELETE)
//
"""
sys.stdout.write(j)

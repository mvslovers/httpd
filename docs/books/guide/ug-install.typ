#import "../bookmaster/bookmaster.typ": *

= Installing HTTPD <ug-install>

#idx("installation")
This chapter installs HTTPD on an MVS 3.8j system (TK4-, TK5, MVS/CE or a
Hercules build of your own) from the release package. The installation is done
by SMP Release 4, the SMP that comes with MVS 3.8j, not SMP/E. SMP keeps a
record of what was installed. So there is a defined way to replace it with the
next release (@ug-upgrade) and a defined way to take it out (@ug-remove).

The captures in this chapter come from release 4.4.0 on a TK5 system, with
two exceptions that the text names: the RAKF captures come from MVS/CE. The
file names in your package carry the number of your release. The steps do not
change from one release to the next.

Report a problem as an issue at https://github.com/mvslovers/httpd/issues.
Add the two build stamps that HTTPD writes at the start (#cmd("HTTPD000I") and
#cmd("HTTPD005I"), @ug-install-start), the console messages and, for an
installation problem, the job output. Questions go to the Discord server of
the project.

The names of the data sets carry no version. #cmd("HTTPD.LINKLIB") is the same
name in every release. The #cmd("STEPLIB") of the procedure and an APF entry
are written once. The price is that two releases cannot be installed side by
side. An upgrade replaces what is there, and there is no way back to the
previous release: the install job accepts the new level, and SMP refuses a
#cmd("RESTORE") of an accepted function.

The installation is a sequence of tasks. Each one has a goal, what you type,
what comes back and what can go wrong:

+ Read what is in the package (@ug-install-files) and what the system needs
  (@ug-install-prereq).
+ Send the two XMIT files to MVS (@ug-install-upload).
+ Stop a running HTTPD, if there is one (@ug-install-stop).
+ Allocate the product data sets (@ug-install-alloc).
+ Edit and submit the install job (@ug-install-job), read the result
  (@ug-install-result) and verify the work (@ug-install-verify).
+ Put the procedure and the Parmlib member in place (@ug-install-proc).
+ Define the user and the profiles in RAKF (@ug-install-rakfsetup).
+ Put the webroot disk on the system and mount it (@ug-install-webroot).
+ Start HTTPD (@ug-install-start).

If this system already runs an older HTTPD, do not use this chapter. Use
@ug-upgrade.

== What Is in the Package <ug-install-files>

#idx("package", "contents")
The package is the archive #cmd("httpd-4.4.0-dist.zip") (or
#cmd("httpd-4.4.0-dist.tar.gz")). It has six files. In their names, 4.4.0 is
the release. The webroot image has no version in its name.

#tab(caption: [The files of the installation package])[
  #table(columns: (2.7in, 1fr),
    [File], [What it is],
    [#cmd("README.md")], [The installation guide as a text file.],
    [#cmd("httpd-4.4.0-load.xmit")], [The five load modules, in a form that
      TSO RECEIVE reads. A little over 800,000 bytes.],
    [#cmd("httpd-4.4.0-samplib.xmit")], [The sample library: the procedure of
      the started task, a Parmlib member and the job that allocates the
      webroot disk. About 21,000 bytes.],
    [#cmd("httpd-4.4.0-alloc.jcl")], [Allocates the two libraries that SMP
      installs into. Run it once.],
    [#cmd("httpd-4.4.0-inst.jcl")], [The install job: TSO RECEIVE of both
      XMIT files, then SMP RECEIVE, APPLY and ACCEPT.],
    [#cmd("httpd-webroot.img")], [The webroot disk: a 1 MB file system image
      with the welcome page. It is not part of the SMP installation
      (@ug-install-webroot).],
  )
]

The load XMIT holds five load modules. Only the first is the server:

#tab(caption: [The load modules])[
  #table(columns: (1.3in, 1fr),
    [Module], [What it is],
    [#cmd("HTTPD")], [The server. It is the only module that the started task
      names.],
    [#cmd("HTTPDSRV")], [A display module for a browser. Route
      #cmd("/.dsrv").],
    [#cmd("HTTPDM")], [A display module for a terminal client. Route
      #cmd("/.dm").],
    [#cmd("HTTPDMTT")], [A display module for the master trace table. Route
      #cmd("/.dmtt").],
    [#cmd("ABEND0C1")], [A module that abends on purpose, for testing. Route
      #cmd("/.abend").],
  )
]

Nothing uses the last four until a #cmd("MOD=") line in the Parmlib member
names them, and the shipped member has those lines as comments. They are
diagnostic tools and need an #cmd("AUTH=") on the route. "The Display
Modules" in the _HTTPD Reference_ describes them.

The sample library holds three members:

#tab(caption: [The members of the sample library])[
  #table(columns: (1.3in, 1fr),
    [Member], [What it is],
    [#cmd("HTTPD")], [The procedure of the started task
      (@ug-install-proc).],
    [#cmd("HTTPPRM0")], [A Parmlib member to copy and edit.],
    [#cmd("HTTPWEBR")], [A job that allocates the data set for the webroot
      disk (@ug-install-webroot).],
  )
]

The installation creates these data sets. Their names are the same in every
release:

#tab(caption: [The data sets of the installation])[
  #table(columns: (1.9in, 1fr),
    [Data set], [Role],
    [#cmd("HTTPD.LINKLIB")], [The load library. SMP owns it. The
      #cmd("STEPLIB") of the procedure names it, and it can be on the APF
      list. Allocated by the allocation job.],
    [#cmd("HTTPD.AHTTPLOD")], [The distribution library: the copy that SMP
      keeps for the accepted level. Allocated by the allocation job.],
    [#cmd("HTTPD.HTTPLOAD")], [The staging library. TSO RECEIVE creates it,
      SMP reads the modules from it, and the last step of the install job
      scratches it.],
    [#cmd("HTTPD.SAMPLIB")], [The sample library. TSO RECEIVE creates it. It
      is a plain library that SMP does not own.],
    [#cmd("HTTPD.WEBROOT.UFS")], [The webroot disk. The job #cmd("HTTPWEBR")
      allocates it. SMP does not own it, and no upgrade touches it.],
  )
]

== What You Need <ug-install-prereq>

#idx("prerequisites")
- MVS 3.8j on Hercules, with SMP 4 in working order: the procedures
  #cmd("SMPREC") and #cmd("SMPAPP") and the #cmd("SYS1.SMP*") data sets. They
  are there on TK4-, TK5 and MVS/CE as delivered.
- RAKF, for the logins (@ug-install-rakf). This one is not optional.
- UFSD, only if you want to serve static files (@ug-install-ufsd).
- mvsMF, only if you want the REST API that the shipped Parmlib member routes
  to (@ug-install-mvsmf).
- A user ID that may submit jobs, change the procedure library of the started
  tasks and change a PARMLIB.
- A way to send a file to the host in binary (@ug-install-upload).
- Disk space for the four data sets of SMP and TSO RECEIVE. The allocation job
  asks for 5 cylinders for each of the two libraries. The staging library is
  about the size of the load XMIT and goes away again at the end of the install
  job.

=== UFSD Is Optional <ug-install-ufsd>

#idx("UFSD")
HTTPD serves static files from the file system of UFSD, and from nowhere
else. The setting #cmd("DOCROOT") names a path in that file system. Without
UFSD there is nothing to serve statically.

Nothing about the installation needs UFSD. The SYSMOD declares no
prerequisite. A UFSD that cannot be reached at the start is a warning and not
a failure:

#screen(```
HTTPD044W UNABLE TO INITIALIZE FILE SYSTEM
```)

The server starts and listens, and every #cmd("MOD=") route works. Only the
requests for static files fail. Beware of one thing: #cmd("HTTPD001I") still
says #cmd("READY - SERVING /www") in this case. The message names the
document root and does not say that the file system is there. If you want a
server without static files, put #cmd("UFS=0") in the Parmlib member. That
skips the initialisation and the warning ("The HTTPPRM Keywords" in the
_HTTPD Reference_).

If you want static files, UFSD must run before HTTPD, as a started task of its
own. HTTPD 4.5 is built against UFSD 1.5.2 (HTTPD 4.4.0 against 1.5.1). A UFSD older than 1.5.0 is
untested. The _UFSD User's Guide_ installs UFSD.

=== mvsMF Is Optional <ug-install-mvsmf>

#idx("mvsMF")
The shipped Parmlib member has four active routes, and all of them go to the
server module #cmd("MVSMF"): #cmd("/zosmf/info"),
#cmd("/zosmf/services/authenticate"), #cmd("/zosmf/restconsoles/*") and
#cmd("/zosmf/*").

HTTPD loads a server module by name with the LINK SVC. The module must be in
the #cmd("STEPLIB") of the started task or in the link list. Install mvsMF
from its own package, which has its own guide, and put its library there. If
HTTPD does not find the module, a request for one of the four paths gets an
error response, and the console shows

#screen(```
HTTPD908E EXTERNAL PROGRAM MVSMF ...
```)

(the rest of the line says why). If you do not use mvsMF, comment out or
delete the four lines. Then no request reaches a module.

=== RAKF Is Required for Logins <ug-install-rakf>

#idx("RAKF")
HTTPD has no user database. A login with a user ID and a password is verified
by RAKF, and the access to a protected resource is checked against RAKF
profiles under the identity of the user who is logged in (@ug-security). The
started task itself also needs a user ID in RAKF (@ug-install-rakfsetup). On a
system without RAKF nobody can log in, and no setting turns that off.

=== Authority <ug-install-auth>

#idx("authorization")
#idx("APF")
The load module #cmd("HTTPD") is linked with AC(1), and the server needs
APF authorization, because without it the LINK SVC and the security calls are
not available. SMP copies the module and does not bind it again, so the
attribute stays. There are two ways to get the authorization.

*Through the APF list.* Add #cmd("HTTPD.LINKLIB") to
#cmd("SYS1.PARMLIB(IEAAPF00)"). Each line has the data set name and the
volume serial, separated by a blank. The previous last line gets a trailing
comma. MVS 3.8j reads the APF list at the IPL only, and there is no dynamic
APF list on TK5, so the entry costs one IPL. The name of the library does
not change in later releases, so it costs only that one. The start shows

#screen(```
HTTPD002I AUTHORIZED BY LIBRARY (MODULE KEY 0)
```)

*Through RAKF.* RAKF supplies the SVC 244, and HTTPD calls it itself at the
start to get authorized. You need RAKF anyway, so this needs no other setup.
The start shows

#screen(```
HTTPD002I AUTHORIZED BY SVC (MODULE KEY 8)
```)

The capture of @ug-install-start is this second case.

An APF entry on MVS 3.8j names the data set and the volume. The entry does
not only permit a volume, it dictates one. The allocation job says
#cmd("UNIT=SYSDA") and no volume, so it puts the library wherever
#cmd("SYSDA") picks, and the next section shows a second run that put both
libraries on other volumes than the first. If the volume is not the one in the
entry, you get a library that exists, holds the right module and is not
authorized. Pin the volume on the #cmd("LINKLIB") statement of the allocation
job:

#screen(```
//LINKLIB  DD  DSN=HTTPD.LINKLIB,DISP=(NEW,CATLG,DELETE),
//            UNIT=SYSDA,VOL=SER=your-apf-volume,
```)

Nothing reports the mismatch, because HTTPD recovers from it: it falls back to
the SVC and comes up authorized. The only place where the two ways can be told
apart is #cmd("HTTPD002I"). So #cmd("AUTHORIZED BY SVC") on a system where you
added an APF entry is not a decision of the site but a symptom.

If neither way gives the authorization, the start writes

#screen(```
HTTPD012E HTTPD UNABLE TO DYNAMICALLY OBTAIN APF AUTHORIZATION
```)

The first word after the message number is the name of the program. There is
no #cmd("HTTPD002I"), and the step ends with a return code that is not zero.
"The Started Task and the Package" in the _HTTPD Reference_ has the whole
story of the authorization, also for the modules that run in the task.

== Sending the Two XMIT Files <ug-install-upload>

#idx("upload", "XMIT")
#idx("FTP")
Goal: the two XMIT files exist as data sets on the MVS system. Both are
sequential data sets with fixed blocks of 80 bytes (#cmd("RECFM=FB"),
#cmd("LRECL=80")), and both must arrive in binary. A text transfer changes
the bytes, and then TSO RECEIVE cannot read the file. The data set names are
your choice. The captures use #cmd("HERC01.HTTPD.LOAD.XMIT") and
#cmd("HERC01.HTTPD.SAMP.XMIT").

*By FTP.* If FTPD or another FTP server runs on the system, switch to
binary and send both files:

#screen(raw(read("../ex/ug-install/upload-ftp.txt")))

Read the capture from the top. After #cmd("binary") the server answers
#cmd("Representation type is Image"), which is the binary mode. Each
#cmd("put") ends with #cmd("250 Transfer completed successfully"). The byte
count must equal the size of the file on your disk. The listing at the end
shows the result on the host: #cmd("FB"), 80, a block size of 3120, and 18
tracks for the load XMIT. If the record format or the length is something
else, the transfer was not binary.

*By mvsMF and zowe.* If mvsMF runs, the zowe CLI can upload a file to a data
set in binary mode. If you upgrade, use FTP instead: mvsMF is the server that
you have to stop.

*By IND\$FILE.* A 3270 emulator with the IND\$FILE transfer can send the file in
binary, without line-end conversion. Allocate the data set first as
#cmd("FB") with a record length of 80, so that the label has the right
record format.

#note[Only the FTP route is captured in this book. The zowe and IND\$FILE
routes are described here without a session.]

*What can go wrong.* A text transfer, or a data set with another record format.
The RECEIVE step of the install job then fails. Send the file again in binary,
into a data set that does not yet exist.

== Stopping a Running HTTPD <ug-install-stop>

#idx("stop")
Goal: nothing has HTTPD's libraries open. Skip this task on a first
installation.

The allocation job creates #cmd("HTTPD.LINKLIB"), which a running HTTPD has
open through its #cmd("STEPLIB"). The install job writes into it. A job that
cannot get the library waits. The console shows the wait, the stop and the
end of the wait:

#screen(raw(read("../ex/ug-install/alloc-exists-console.txt")))

The first lines are the wait: #cmd("IEF861I") and #cmd("IEF863I") name the
data set that the job cannot get, and #cmd("IEF099I") says the job waits.
Stop HTTPD with #cmd("P HTTPD"). The job goes on as soon as the task has
ended.

The stop is a normal MVS stop. The message #cmd("HTTPD098I") starts it, the
threads end one by one (#cmd("HTTPD060I")), and #cmd("HTTPD416I") prints the
counters of the run. #cmd("HTTPD099I") says that the shutdown is complete,
and #cmd("$HASP395 HTTPD ENDED") is the line that tells you the address space
is gone. Wait for it before you go on. There is no
#cmd("F HTTPD,SHUTDOWN"), although the comments in the procedure still list it
("Console Commands" in the _HTTPD Reference_).

The capture is of a second run of the allocation job on a system that already
had the libraries. It ends with a good-looking return code, and the next
section explains why that is not good.

== Allocating the Product Data Sets <ug-install-alloc>

#idx("allocation")
Goal: the two libraries that SMP installs into exist and are cataloged. Run
the allocation job #cmd("httpd-4.4.0-alloc.jcl") once, on a system that has no
HTTPD yet. It allocates a program library with undefined records
(#cmd("RECFM=U"), #cmd("BLKSIZE=15040")) for each of #cmd("HTTPD.LINKLIB") and
#cmd("HTTPD.AHTTPLOD"). Each has 5 cylinders as primary space, 2 as secondary
and 20 directory blocks. These two libraries are the data sets of this chapter
that SMP owns.

The job has no #cmd("DELETE") step on purpose. After the SYSMOD is accepted,
the distribution library holds the accepted copy of every module. A rerun that
scratched it would leave the SMP records saying "accepted" with an empty
library behind them, and nothing would tell you. The comment in the job gives
the rule: if you must start over, cut the FMID out with UCLIN (@ug-remove), and
then delete the data sets by hand. An accepted function cannot be rejected or
restored.

Edit #cmd("UNIT") and #cmd("VOL=SER") if #cmd("SYSDA") is not what you want.
Pin the volume if you use the APF list (@ug-install-auth). Submit the job. A
good run looks like this:

#screen(raw(read("../ex/ug-install/alloc-console.txt")))

The step is #cmd("IEFBR14") and the return code is #cmd("0000"). The system
messages of the job say where the data sets went:

#screen(raw(read("../ex/ug-install/alloc-sysmsg.txt")))

Read #cmd("IEF285I"). Both data sets are #cmd("CATALOGED"), and
#cmd("VOL SER NOS=") names the volumes. Write down the volume of
#cmd("HTTPD.LINKLIB") if you use the APF list.

*What can go wrong: the data set exists.* Run the job on a system that has
the names already, and it also ends with condition code 0000. The system
messages show the difference:

#screen(raw(read("../ex/ug-install/alloc-exists-sysmsg.txt")))

#cmd("IEF287I") with #cmd("NOT CATLGD 2") says that the new data set was
created and could not be cataloged, because the name is taken. Note the
volumes: the new copies are on the other volumes than in the first run. They are
not the libraries that the system uses. A return code of 0000 says that the
job ran. It does not say that the work happened. Look at #cmd("IEF285I") and
#cmd("IEF287I") every time, and delete the uncataloged copies by hand.

*What can go wrong: a running HTTPD.* The job waits (@ug-install-stop).

*What can go wrong: SB37.* The space is in cylinders on purpose. A primary
in tracks reaches the 16-extent limit long before the volume is full, and the
abend that follows reads like a full volume.

== Editing and Submitting the Install Job <ug-install-job>

#idx("install job")
Goal: the install job runs with the names of your two XMIT data sets. Open
#cmd("httpd-4.4.0-inst.jcl"). The only lines that you have to change are the
two #cmd("RECEIVE") statements, in the steps #cmd("RECV1") and
#cmd("RECV2"). They name the data sets that you sent. The capture is the
difference between the shipped job and the edited one:

#screen(raw(read("../ex/ug-install/inst-jcl-edit.txt")))

Keep the rest. The job has eight steps:

#tab(caption: [The steps of the install job])[
  #table(columns: (1.1in, 1fr),
    [Step], [What it does],
    [#cmd("DELOLD")], [Deletes #cmd("HTTPD.HTTPLOAD") and
      #cmd("HTTPD.SAMPLIB"), the two targets of the next steps. TSO RECEIVE
      refuses to merge into an existing data set, and without this step the
      job would run only once.],
    [#cmd("RECV1")], [TSO RECEIVE of the load XMIT into
      #cmd("HTTPD.HTTPLOAD").],
    [#cmd("RECV2")], [TSO RECEIVE of the sample XMIT into
      #cmd("HTTPD.SAMPLIB").],
    [#cmd("RECV")], [SMP RECEIVE. The SYSMOD travels inside the job and
      goes into the SMP inventory.],
    [#cmd("APPLYCHK")], [SMP APPLY with CHECK: a dry run that changes
      nothing.],
    [#cmd("APPLY")], [SMP APPLY: copies the five modules into
      #cmd("HTTPD.LINKLIB").],
    [#cmd("ACCEPT")], [SMP ACCEPT: copies them into
      #cmd("HTTPD.AHTTPLOD") and makes this level the one a
      #cmd("RESTORE") would return to.],
    [#cmd("CLEANUP")], [Scratches the staging library
      #cmd("HTTPD.HTTPLOAD").],
  )
]

The steps depend on each other through condition codes. The step #cmd("RECV")
runs if #cmd("RECV2") ended with 0. #cmd("APPLYCHK") runs if #cmd("RECV")
ended with 0, and #cmd("APPLY") runs if #cmd("APPLYCHK") ended with 0. So the
apply does not start when the dry run found a problem. #cmd("ACCEPT") runs if
#cmd("APPLY") ended with 4 or lower, and #cmd("CLEANUP") only if
#cmd("ACCEPT") ended with 0.

The SYSMOD is part of the job as an inline data set, in a
#cmd("DD DATA,DLM=@@") statement. It is plain text, so there is no third file
to send and no third data set to allocate. The terminator is #cmd("@@") and
not the usual #cmd("/*"), because the SYSMOD contains statements that start
with #cmd("//") and a copy statement that ends with #cmd("/*").

The steps #cmd("APPLYCHK"), #cmd("APPLY") and #cmd("ACCEPT") carry a note in
the job: the DDs that override one of the procedure #cmd("SMPAPP") must come
before the DDs that the step adds. In the wrong order, SMP quietly uses the
data set of the procedure, and the messages still name the one you wanted. Do
not reorder those statements.

Submit the job. From a workstation, FTP can do it with the file type JES:

#screen(raw(read("../ex/ug-install/submit-ftp.txt")))

The command #cmd("quote site filetype=jes") switches the FTP server to job
submission. The #cmd("put") then sends the file to the internal reader of JES,
and the line #cmd("250-It is known to JES as JOBnnnnn") names the job. You can
also submit the job from TSO, from the mvsMF job API or from a card reader.
Under a minute later the job is done.

== Reading the Result <ug-install-result>

#idx("return code")
Goal: know whether the install job did its work. The console shows the step
summary:

#screen(raw(read("../ex/ug-install/inst-console.txt")))

Every step ends with #cmd("RC= 0000"), and the line at the end says
#cmd("MAX COND CODE 0000"). The job log has the same table with the times:

#screen(raw(read("../ex/ug-install/inst-joblog.txt")))

The return codes mean this:

- *0000.* The step did what it says.
- *0004.* A warning. It is normal in one place: the #cmd("APPLY") step of an
  upgrade ends with 0004 (@ug-upgrade). A first installation ends with 0000
  in every step.
- *0008 or higher.* The step failed. The conditions stop the later steps, so
  the first step with a high code is the one to read.

The messages of every step are in the job output. The steps in order:

*DELOLD.* The step ends with 0000, although the capture looks like a failure:

#screen(raw(read("../ex/ug-install/inst-delold.txt")))

The first delete says #cmd("NOT DELETED") and ends with condition code 8,
because #cmd("HTTPD.HTTPLOAD") does not exist yet. The statement
#cmd("SET MAXCC=0") then makes the step end with 0. This is normal and not an
error.

*RECV1 and RECV2.* TSO RECEIVE unpacks an XMIT file into a data set:

#screen(raw(read("../ex/ug-install/inst-recv-load.txt")))

The line #cmd("Receive into 'HTTPD.HTTPLOAD' successful") is the one to
find. The name after #cmd("Receiving") is the name the XMIT had on the
machine where it was made, and it is not the name of your data set. The
sample library has the same shape:

#screen(raw(read("../ex/ug-install/inst-recv-samplib.txt")))

*RECV.* SMP reads the SYSMOD from the inline data set and stores it. The
output repeats the statements and ends with a return code. In it you see the
function #cmd("THTP440"), the statement #cmd("++VER(Z038) DELETE(THTP430)"),
and one #cmd("++MOD") statement for each of the five modules.

#screen(raw(read("../ex/ug-install/inst-smp-receive.txt")))

*APPLYCHK.* The dry run. Its output has the same reports as the real run and
changes nothing.

*APPLY.* The real work. Look for the five #cmd("HMA2380") lines:

#screen(raw(read("../ex/ug-install/inst-smp-apply.txt")))

Each #cmd("HMA2380 COPY SUCCESSFUL") line names a module, the library
#cmd("LINKLIB") and the SYSMOD, and ends with #cmd("RETURN CODE=00"). The five
lines are the proof that five modules were copied. The final line
#cmd("HMA2050 ... HIGHEST RETURN CODE IS 00") is only a summary. The copy is
done by IEBCOPY, which SMP starts. Its output lists the same members and says
how many tracks and directory blocks of the library are still unused:

#screen(raw(read("../ex/ug-install/inst-apply-iebcopy.txt")))

*ACCEPT.* The same copy, into the distribution library
#cmd("HTTPD.AHTTPLOD"). The output has five #cmd("HMA2380") lines with
#cmd("LIBRARY=AHTTPLOD"), and it ends with #cmd("HMA2270 ACCEPT PROCESSING
SUCCESSFULLY COMPLETED"):

#screen(raw(read("../ex/ug-install/inst-smp-accept.txt")))

*CLEANUP.* The staging library goes away:

#screen(raw(read("../ex/ug-install/inst-cleanup.txt")))

The system of these captures held the predecessor #cmd("THTP430") in its SMP
records as a deleted level, from an earlier release. That is why the reports
in the captures say #cmd("THTP430 DELETED"). On a system that never had HTTPD,
the reports show only the new function. The return codes are the same.

== Verifying the Install <ug-install-verify>

#idx("verification")
Goal: prove that the modules are in the library and that SMP knows them. A
return code says that the job ran. It does not say that the work happened.

*The members of the library.* List the directory of #cmd("HTTPD.LINKLIB"). In
TSO, #cmd("LISTDS 'HTTPD.LINKLIB' MEMBERS") does it. The utility IEHLIST
prints more. Its output for the library of the capture:

#screen(raw(read("../ex/ug-install/verify-linklib-list.txt")))

There must be five members: #cmd("ABEND0C1"), #cmd("HTTPD"), #cmd("HTTPDM"),
#cmd("HTTPDMTT") and #cmd("HTTPDSRV"). The last line says that 19 of the 20
directory blocks are completely unused. The first line names the volume where
the library is.

*The SMP record.* Ask SMP for the SYSMOD and for one load module. The job is
the procedure #cmd("SMPAPP") with these control statements:

#screen(```
//LISTCDS  EXEC SMPAPP
//SMPCNTL  DD  *
 RESETRC .
 LIST CDS SYSMOD(THTP440) .
 LIST CDS LMOD(HTTPD) .
/*
```)

The output:

#screen(raw(read("../ex/ug-install/verify-smp-list.txt")))

Read the capture for four facts. The status is #cmd("REC  APP  ACC"): the
SYSMOD is received, applied and accepted. The line #cmd("DELETE VER(001)")
names the predecessor that the SYSMOD replaces. The line #cmd("MOD =") lists
the five modules. And #cmd("LASTUPD = THTP440") for the load module
#cmd("HTTPD"), with #cmd("SYSTEM LIBRARY = LINKLIB"), says that the module in
the load library comes from this level.

If the member list is short, or the status lacks #cmd("ACC"), the install did
not do its work, whatever the return codes said.

== The Procedure and the Parmlib Member <ug-install-proc>

#idx("procedure")
#idx("PARMLIB")
Goal: the procedure #cmd("HTTPD") is in a procedure library of the started
tasks, and a Parmlib member exists for it. The sample library has both. Copy
the procedure with IEBGENER:

#screen(```
//COPY     EXEC PGM=IEBGENER
//SYSPRINT DD  SYSOUT=*
//SYSUT1   DD  DISP=SHR,DSN=HTTPD.SAMPLIB(HTTPD)
//SYSUT2   DD  DISP=SHR,DSN=SYS2.PROCLIB(HTTPD)
//SYSIN    DD  DUMMY
```)

#cmd("SYS2.PROCLIB") is an example. Use the procedure library of the started
tasks on your system. Copy #cmd("HTTPPRM0") the same way into the Parmlib.
In the shipped member all keywords are comments, except the four mvsMF routes
of @ug-install-mvsmf. Edit it for your system ("The HTTPPRM Keywords" in the
_HTTPD Reference_ describes every keyword, and @ug-config the way to choose
them).

The procedure starts with four symbolics:

#screen(```
//HTTPD    PROC M=HTTPPRM0,
//            D='SYS2.PARMLIB',
//            STCUSER=HTTPD,
//            STCGRP=USER
```)

#deflist(width: 1.0in,
  [#cmd("M")], [The member of the Parmlib. The default is #cmd("HTTPPRM0").
    Start with another member by #cmd("S HTTPD,M=HTTPPRM1").],
  [#cmd("D")], [The data set of the Parmlib. The default is
    #cmd("SYS2.PARMLIB"). Check that the data set named by D exists on your
    system. If you use another one, change the default in the procedure, or
    name the data set on every start with #cmd("S HTTPD,D='...'").],
  [#cmd("STCUSER")], [The user ID that the server logs on as at the start
    (@ug-install-rakfsetup).],
  [#cmd("STCGRP")], [The group of that user. The name of the symbolic is
    #cmd("STCGRP"), but the keyword in the PARM is #cmd("STCGROUP"): a
    symbolic parameter name is at most 7 characters long.],
)

The program statement is

#screen(```
//HTTPD    EXEC PGM=HTTPD,REGION=8M,TIME=1440,
//            PARM='STCUSER=&STCUSER STCGROUP=&STCGRP'
```)

The #cmd("REGION=8M") is the stock value. A server that is to run many REXX or
Lua requests at the same time needs more, and #cmd("MAXTASK") must not be
raised without it: "The Started Task and the Package" in the _HTTPD Reference_
gives the sizes, and "Troubleshooting" the symptom.

The PARM is separated by blanks, not by commas. A #cmd("CONFIG=") in the PARM
is a leftover of an old release. It gives #cmd("HTTPD024W") and
#cmd("HTTPD024I"), and the server takes its configuration from the DD
#cmd("HTTPPRM") as usual.

*The data definitions.* The procedure ships with these DD statements:

#tab(caption: [The DD statements of the procedure])[
  #table(columns: (1.5in, 1fr),
    [DD], [Purpose],
    [#cmd("STEPLIB")], [#cmd("HTTPD.LINKLIB"). The load library of the server
      and of its modules. The package build writes the name into the
      procedure.],
    [#cmd("SYSUDUMP"), #cmd("SNAP")], [Dump output.],
    [#cmd("HTTPDERR"), #cmd("HTTPDOUT"), #cmd("HTTPDBG"), #cmd("HTTPSTAT")],
      [Output of the server, to the spool.],
    [#cmd("HTTPDIN")], [Dummy.],
    [#cmd("HTTPPRM")], [The Parmlib member: #cmd("DSN=&D(&M)"),
      #cmd("DISP=SHR"), #cmd("FREE=CLOSE").],
    [#cmd("HASPCKPT"), #cmd("HASPACE1")], [Comment lines. The JES2 checkpoint
      and the spool. Optional: see below.],
  )
]

Do not add the DDs #cmd("SYSPRINT"), #cmd("SYSTERM") and #cmd("SYSIN"). The
server refuses to start with them: #cmd("HTTPD014E"), #cmd("HTTPD015E") and
#cmd("HTTPD016E") name them. The messages #cmd("HTTPD017W"), #cmd("HTTPD018W")
and #cmd("HTTPD019W") report that #cmd("HTTPDOUT"), #cmd("HTTPDERR") or
#cmd("HTTPDIN") is missing, and they also stop the start. "Messages" in the
_HTTPD Reference_ lists them.

*HASPCKPT and HASPACE1.* HTTPD itself never opens them. They are for the
server modules. A module runs in the task of HTTPD and has no allocations of
its own, and the jobs API of mvsMF reads the JES2 checkpoint and spool in that
task. The shipped procedure has both statements as comment lines, and you
need them in two cases only:

- You run mvsMF 1.3.0 or earlier. It opens the two data sets by their DD names.
  Without the statements, a job list answers 500, and a request for one job by
  name and id answers 404 "job not found", a spool read included, although the
  job exists. The console shows
  #cmd("MVSMF201E UNABLE TO OPEN THE JES2 CHECKPOINT AND SPOOL DATA SETS").
- The JES2 of your system builds its data set names with a prefix other than
  #cmd("SYS1") (its #cmd("$DSNPRFX") value). From mvsMF 1.4.0 on, the jobs API
  allocates #cmd("SYS1.HASPCKPT") and #cmd("SYS1.HASPACE") itself when the
  procedure has no DD, and it looks only for these names.

In both cases, remove the two comment marks. The values in the shipped lines
(#cmd("UNIT=3350") with #cmd("VOL=SER=MVS000") and #cmd("VOL=SER=SPOOL1")) are
site values. Compare them with the JES2 procedure of your system before the
first start, and change them to what JES2 uses. With mvsMF 1.4.0 or later on a
system with the prefix #cmd("SYS1"), leave the lines as they are. A DD in the
procedure wins over the allocation, so active lines do no harm either.
"The JES2 data sets" in the _HTTPD Reference_ has the details.

*Missing HTTPPRM.* If the DD #cmd("HTTPPRM") does not exist, the server writes

#screen(```
HTTPD020W CANNOT OPEN DD:HTTPPRM -- USING DEFAULTS
```)

and starts with built-in defaults: port 8080 and no routes. A member that
opens and cannot be read to the end gives #cmd("HTTPD038E"), and the server
does not start.

== Defining the Users and Profiles in RAKF <ug-install-rakfsetup>

#idx("RAKF", "users and profiles")
Goal: RAKF knows the identity of the started task and the profiles that the
shipped routes name. This task has two parts, and the first matters more.

Which RAKF do you have? TK4- and TK5 have RAKF 1.2.x. MVS/CE has RAKF 2.0.0.
The two differ in what you type:

#tab(caption: [RAKF 1.2.x and 2.0.0])[
  #table(columns: (1.6in, 1fr, 1fr),
    [], [RAKF 1.2.x (TK4-, TK5)], [RAKF 2.0.0 (MVS/CE)],
    [Password in the table USERS], [Required], [Blank],
    [Where the password is kept], [In clear text, in
      #cmd("SYS1.SECURE.CNTL(USERS)")], [Not in the table],
    [Profiles], [Edit the member by hand], [#cmd("RDEFINE") and
      #cmd("PERMIT")],
  )
]

=== The User of the Started Task

RAKF has no table of started tasks. Every started task runs under the same
account, #cmd("STC") in the group #cmd("STCGROUP"), and on the stock profiles
that group holds ALTER on every data set on the system. HTTPD replaces this
identity at the start. It logs on the user #cmd("STCUSER") in the group
#cmd("STCGRP") with RACINIT, without a password, and writes

#screen(```
HTTPD004I STC IDENTITY SET TO HTTPD/USER VIA RACINIT
```)

The capture of @ug-install-start shows it. If the user is not defined, the
logon fails. The server writes #cmd("HTTPD004W") with the user, the group and
a return code, and goes on. It then runs as the account it inherited, with
far more access than you meant. A warning is all you get, and that is the
danger. So a start without #cmd("HTTPD004I") is not a good start.

Add a line to the table #cmd("SYS1.SECURE.CNTL(USERS)"). The columns are fixed:
the user ID in 1 to 8, the group in 10 to 17, the password in 19 to 26, the
Operations flag in 28 and the Special flag in 30. The first two lines in the
next examples are a ruler and are not part of the member. On TK5, with a
placeholder password that you replace:

#screen(```
         1         2         3
123456789012345678901234567890
HTTPD    USER     pppppppp N
```)

On MVS/CE, where the password stays blank:

#screen(```
         1         2         3
123456789012345678901234567890
HTTPD    USER              N
```)

Rules for the line:

- Choose the password as you would for any account of that group. HTTPD never
  uses it, because it logs on without one, but the account exists and can be
  tried. On RAKF 1.2.x the table keeps passwords in clear text, so give READ on
  #cmd("SYS1.SECURE.CNTL") only to the people who administer RAKF.
- On RAKF 1.2.x a blank password is refused when the table is loaded.
  #cmd("S RAKFUSER") ends with #cmd("RAKFUIDS2 INPUT DATA INVALID OR OUT OF SEQ."),
  then #cmd("RAKFUIDSX ** PROGRAM TERMINATED **") and an abend U0200. The echo
  of the card always shows #cmd("********") in the password column, empty or
  not, so the echo does not help. Nothing in the table is updated.
- Keep Operations at #cmd("N"). With #cmd("Y") the account has the
  operations authority, which is access to data sets without a profile.
- Both tables must be in ascending order. A line out of order does not skip
  that line: the table is not initialised at all, and no one gets
  authorization.
- Keep #cmd("STCGRP") equal to the group on the #cmd("USERS") line, so that
  the two do not drift apart. RAKF takes the group that you request at the
  logon and does not check that the user is connected to it.

The user needs READ on the Parmlib data set and on whatever HTTPD opens before
a client identity exists. The stock rule for data sets gives that. If you
narrowed it, give the group READ on the Parmlib data set, or the start fails.
The user does not need a profile for SVC 244: HTTPD returns the authorization
under the account it took it with, not under #cmd("STCUSER").

Another name or group goes into the start command:
#cmd("S HTTPD,STCUSER=name,STCGRP=group"), or into the defaults of the
procedure.

=== The Profiles

Every route of the Parmlib member that has a #cmd("RES=class:resource")
needs a profile for that resource. The shipped member has one, the mvsMF
console service:

#screen(```
MOD=MVSMF /zosmf/restconsoles/*          AUTH=TOKEN RES=FACILITY:MVSMF.CONSOLE
```)

The profile is checked against the identity of the client on every request,
so the user of the started task needs none of them. A route whose resource has
no profile is not denied. RAKF treats a resource without a profile as
allowed, the check of the resource does nothing, and only the check of
#cmd("AUTH=") is left. HTTPD says so at the start:

#screen(```
HTTPD425W NO PROFILE FOR FACILITY:MVSMF.CONSOLE -- /zosmf/restconsoles/* NOT GATED
```)

With the shipped member this is what a system without the profile prints. Until
the profile exists, every user who can log in may issue operator commands
through mvsMF up to 1.3.0. From mvsMF 1.4.0 on, mvsMF checks the same profile
itself and refuses every user until it exists, so the warning then concerns
only this route. Define it. A start without the message does not prove that the profile is
right. It proves only that the message is not there, so look at the table.

From HTTPD 4.5.0 the keyword #cmd("RES_NOPROFILE=DENY") turns the permissive
reading into a refusal. Once the profiles are defined and the start prints no
#cmd("HTTPD425W"), add the keyword to the member. A route whose profile is
forgotten or not loaded yet then answers 403, and the start prints
#cmd("HTTPD426W NO PROFILE FOR class:resource -- path REFUSES ALL REQUESTS"),
instead of opening the route to everyone who can log in. Leave it off while a
route still relies on the permissive reading. @ug-security-noprofile has the
details. The shipped member has the keyword as a comment line.

On MVS/CE, the commands #cmd("RDEFINE") and #cmd("PERMIT") define a profile.
Run them in a TSO batch step. The capture defines the console profile, a second
one, #cmd("HTTPD.ADMIN"), which a route such as #cmd("LOC=/admin/*") could
name, and a third, #cmd("HTTPD.DENIED"), that is defined and permitted to
nobody:

#screen(raw(read("../ex/ug-security/rakf-define.txt")))

Each #cmd("RDEFINE") writes the universal line of a profile with
#cmd("UACC(NONE)"). Each #cmd("PERMIT") adds a line that gives READ to a group
or a user, here #cmd("ADMIN"). Run #cmd("RDEFINE") first. The profile is not
active yet: the reply says #cmd("Run RAKFPROF to activate the changes"). Choose
the group of the people who may issue operator commands with care. READ on the
console profile means every operator command, and nothing narrows it per
command.

On TK5 these commands do not exist, and the member
#cmd("SYS1.SECURE.CNTL(PROFILES)") is edited by hand. The columns are fixed:
the class in 1 to 8, the resource in 9 to 52, the group in 53 to 60 and the
permission in 61 to 66. The universal line of a profile has a blank group and
comes first. The lines for the same profiles as above (the first two lines
are a ruler):

#screen(```
         1         2         3         4         5         6
123456789012345678901234567890123456789012345678901234567890123456
FACILITYHTTPD.ADMIN                                         NONE
FACILITYHTTPD.ADMIN                                 ADMIN   READ
FACILITYHTTPD.DENIED                                        NONE
FACILITYMVSMF.CONSOLE                                       NONE
FACILITYMVSMF.CONSOLE                               ADMIN   READ
```)

Merge them into the member in ascending order. A line that is out of order
stops the whole table from loading.

*Reloading.* The tables are read when the RAKF tasks start, so a change needs a
start. The task #cmd("RAKF") reads both tables. #cmd("RAKFPROF") and
#cmd("RAKFUSER") read one each, the profiles and the users. The capture shows
the profile task on MVS/CE:

#screen(raw(read("../ex/ug-install/rakf-reload.txt")))

#cmd("RAKFPROF7  RAKF PROFILES UPDATED") is the line that says the table is
loaded.

*What can go wrong.* A profile for a group the person is not in: the request
is refused with 403. The _HTTPD Reference_ chapter "Routes and Authentication
Policy" has the rules for the route modes, and @ug-security shows how to use
them.

== The Document Root Disk <ug-install-webroot>

#idx("webroot")
#idx("document root")
Goal: UFSD has a file system mounted at the path of #cmd("DOCROOT"), and the
file system holds the welcome page. Skip the task if you do not use static
files, and if you already have a webroot disk that you serve. The data set
#cmd("HTTPD.WEBROOT.UFS") is very likely that disk, and no upgrade touches it.

The package carries the disk as a file, #cmd("httpd-webroot.img"). It is a
1 MB file system image with 256 blocks of 4096 bytes, and it holds one page,
#cmd("index.html"): the welcome page. The image travels outside SMP, because
SMP has no element type for a data set with undefined records, and because
site content is the one thing an install must never replace.

*Allocate the data set.* Submit the job #cmd("HTTPWEBR") from the sample
library: submit the member from #cmd("HTTPD.SAMPLIB") itself. Its important
statement is

#screen(```
//WEBROOT  DD  DSN=HTTPD.WEBROOT.UFS,DISP=(NEW,CATLG),
//             UNIT=SYSDA,SPACE=(4096,256),
//             DCB=(DSORG=PS,RECFM=U,BLKSIZE=4096)
```)

The block size 4096 is not a choice. The image is formatted with it, and UFSD
compares the two when it mounts. A mismatch gives
#cmd("UFSD062E SUPERBLOCK VALIDATION FAILED FOR HTTPD.WEBROOT.UFS"). The space
is counted in blocks of 4096 and has a primary extent only: the file system is
addressed by block number inside the primary extent, so a block in a secondary
extent would never be reached. The job card has no #cmd("NOTIFY") operand on
purpose. That operand is replaced only by TSO SUBMIT and by mvsMF, and any
other way in gets a JCL error.

*Upload the image.* Use IND\$FILE in binary mode, into the data set that the
job allocated. IND\$FILE takes the record format and the block size from the
label of an existing data set, and it invents some only when the data set is
not there. So allocating first is what pins the block size. FTP in binary mode
and the zowe CLI in binary mode are other ways in. They are not measured for
this data set, so the mount is the test. A text transfer destroys the image.

*Mount it.* The mount belongs to UFSD and not to HTTPD. Put the statement
into the Parmlib member of UFSD (#cmd("UFSDPRM0"), unless you named another):

#screen(```
MOUNT    DSN(HTTPD.WEBROOT.UFS)  PATH(/www)  MODE(RO)
```)

and restart UFSD. The statement is the right one for the default
#cmd("DOCROOT=/www"): it keeps the case of the path and creates the mount
point. The operator form of the command, #cmd("F UFSD,MOUNT DSN=...,PATH=..."),
upper-cases the path (it mounts #cmd("/WWW")) and does not create the
directory. Paths are case sensitive, so the default document root finds
nothing under #cmd("/WWW"). The _UFSD User's Guide_ describes both forms and
the command that lists the mounts.

*Verify.* Start HTTPD (@ug-install-start) and request the root path:

#screen(raw(read("../ex/ug-first/welcome-headers.txt")))

The status is #cmd("200 OK") and the type is #cmd("text/html"). The body is
the welcome page, which says "It works. The mainframe is online." @ug-first
reads such an answer line by line.

*Replacing the disk later.* Stop HTTPD and wait for #cmd("$HASP395 HTTPD ENDED").
Stop UFSD. Upload the new image into the same data set. Start UFSD, then
HTTPD.

*What can go wrong.* #cmd("UFSD062E") at the mount: the block size is not
4096, or the file was sent in text mode. A #cmd("404") on every request: the
mount point is not the document root, for example #cmd("/WWW") against
#cmd("/www"). A start with #cmd("HTTPD044W"): UFSD is not running (see
@ug-install-ufsd).

== Starting HTTPD <ug-install-start>

#idx("start")
Goal: the server is up and does what you expect. The checklist before the
first start:

- the load library is on the APF list, or RAKF is running
  (@ug-install-auth);
- the procedure is in the procedure library, and the HASP data sets are right
  (@ug-install-proc);
- the Parmlib member exists and the DD points at it;
- the user and the profiles are in RAKF and the tables are reloaded
  (@ug-install-rakfsetup);
- UFSD runs and has mounted the webroot, if you use static files.

Start the task from the console:

#screen(```
S HTTPD
```)

A start with another member or another Parmlib is #cmd("S HTTPD,M=HTTPPRM1")
or #cmd("S HTTPD,D='MY.PARMLIB'"). A healthy start looks like this:

#screen(raw(read("../ex/ug-install/start-first.txt")))

Read the lines in order:

#deflist(width: 1.0in,
  [#cmd("HTTPD000I")], [The start. The version and the build.],
  [#cmd("HTTPD005I")], [The C library that HTTPD is linked with. Send the
    two lines with a problem report.],
  [#cmd("HTTPD002I")], [How the task got its authorization
    (@ug-install-auth).],
  [#cmd("HTTPD004I")], [The identity of the task now: #cmd("HTTPD/USER").
    Without this line the task runs as the inherited account.],
  [#cmd("HTTPD036I")], [One line for each #cmd("MOD=") route.],
  [#cmd("HTTPD054I")], [The server listens on the port.],
  [#cmd("HTTPD061I")], [One line for the socket thread and each worker.],
  [#cmd("HTTPD001I")], [HTTPD is ready. The text after #cmd("SERVING") is the
    document root.],
)

The member used in the capture had three mvsMF routes. The shipped member has
four active routes, so a start with it shows the lines for the routes that
you have.

A start that does not come up ends the task with a condition code other than
0, and 8 for a refused configuration. A clean stop ends with 0. The messages in front of a refused start say why. The most common
ones:

- #cmd("HTTPD037E HTTPD IS ALREADY ACTIVE ON PORT n, THIS INSTANCE ENDS"):
  another HTTPD already serves the port.
- #cmd("HTTPD030I"): the port is in use. The server waits and tries again.
  After the last try, #cmd("HTTPD030E") ends the instance. The keywords
  #cmd("BIND_SLEEP") and #cmd("BIND_TRIES") set the wait and the number of
  tries. Both default to 10 and are limited to the range 1 to 100.
- #cmd("HTTPD012E"): no APF authorization (@ug-install-auth).
- #cmd("HTTPD411E") followed by #cmd("HTTPD420E"): a route has a mode that does
  not exist.
- #cmd("HTTPD048E"): the retired keyword #cmd("LOGIN") with a value that used
  to require a login. A #cmd("LOGIN") with #cmd("NONE") or no value gives the
  warning #cmd("HTTPD048W") instead. Convert the routes to #cmd("AUTH=").

== Restarting <ug-install-restart>

#idx("restart")
To pick up a change of the Parmlib member, stop and start:

#screen(```
P HTTPD
S HTTPD
```)

Do not start before the stop has finished. Wait for #cmd("$HASP395 HTTPD ENDED").
A start that overlaps the end of the old task finds the port taken
(#cmd("HTTPD030I")). @ug-ops has the operator side: how to watch a running
server, how to change a setting without a restart, and what every command
prints.

== What HTTPD Serves After the Install <ug-install-serves>

#idx("routes", "shipped")
With the shipped member and nothing else installed, HTTPD answers on the port
8080 as follows:

- A request for a path under #cmd("/zosmf/") goes to mvsMF. Two of the four
  routes, #cmd("/zosmf/info") and #cmd("/zosmf/services/authenticate"), have
  #cmd("AUTH=NONE"). The other two need a login token (#cmd("AUTH=TOKEN")).
  Without mvsMF installed, each of these requests gives an error response and
  the message #cmd("HTTPD908E").
- Every other path is a request for a static file, from the document root. With
  the webroot disk mounted, a request for #cmd("/") returns the welcome page.
  Without UFSD, such a request fails.
- A path that no route claims is public. A route without #cmd("AUTH=") is
  public too. So do not add a route without a mode.

The display modules are not reachable. Nothing names them until you add a
#cmd("MOD=") line. @ug-first sends the first requests. @ug-config and
@ug-routes change what is served, and @ug-security says who may fetch it.

== Problems at Installation <ug-install-problems>

#idx("problems", "installation")
@ug-trouble has the general table. The ones that belong to the install:

#tab(caption: [Problems of the installation])[
  #table(columns: (2.2in, 1fr),
    [What you see], [What it means and what to do],
    [The allocation job waits with #cmd("IEF099I")], [A running HTTPD has the
      library. Stop it with #cmd("P HTTPD") (@ug-install-stop).],
    [#cmd("IEF287I ... NOT CATLGD 2") in the allocation job], [The names
      exist. The new copies are not the libraries in use. Delete them by hand
      (@ug-install-alloc).],
    [The RECEIVE step ends with a return code that is not zero], [The XMIT
      file was sent in text mode, or its record format is not FB 80
      (@ug-install-upload).],
    [All steps end with 0 but #cmd("HTTPD.LINKLIB") has no members], [A return
      code says that the job ran. List the members and the SMP record
      (@ug-install-verify).],
    [#cmd("HTTPD012E") at the start], [No APF authorization and no SVC
      (@ug-install-auth).],
    [#cmd("HTTPD002I AUTHORIZED BY SVC") although the library is on the APF
      list], [The library is on another volume than the APF entry
      (@ug-install-auth).],
    [No #cmd("HTTPD004I"), or #cmd("HTTPD004W") instead], [The user of the
      task is not defined in RAKF, or the logon failed
      (@ug-install-rakfsetup).],
    [#cmd("HTTPD425W") or #cmd("HTTPD426W") at the start], [A route names a
      resource that has no profile (@ug-install-rakfsetup).],
    [#cmd("HTTPD044W") at the start], [UFSD is not running
      (@ug-install-ufsd).],
    [#cmd("UFSD062E") at the mount], [The webroot data set has not the block
      size 4096, or the image was sent in text mode (@ug-install-webroot).],
    [#cmd("HTTPD020W") at the start], [The DD #cmd("HTTPPRM") is missing
      (@ug-install-proc).],
    [#cmd("HTTPD014E"), #cmd("HTTPD015E") or #cmd("HTTPD016E") at the start],
      [The procedure has a DD that it must not have (@ug-install-proc).],
  )
]

To take HTTPD out of the system again, follow @ug-remove.

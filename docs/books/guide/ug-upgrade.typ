#import "../bookmaster/bookmaster.typ": *

= Upgrading HTTPD <ug-upgrade>

#idx("upgrade")
This chapter moves a system that runs an older HTTPD to HTTPD 4.5.1. It is
written for a system that HTTPD 4.0.x, 4.1.x, 4.2.x, 4.3.x, 4.4.x or 4.5.0
installed by SMP, and it has a section for a system that still runs
HTTPD 3.x. The release that 4.5.1 replaces in one step is 4.5.0. A system at an
older release goes to 4.5.0 first (@ug-upgrade-hops). A system without HTTPD needs @ug-install, and a system that should
lose HTTPD needs @ug-remove.

The upgrade is the same install job as in @ug-install, with a different
effect. Each release names its predecessor in the SYSMOD. When SMP applies the
new release, it deletes the load modules of the old one from
#cmd("HTTPD.LINKLIB") and copies the new ones in. This is what an upgrade does:

- It replaces the five load modules in place. The names of the data sets
  carry no version, so the #cmd("STEPLIB") of the procedure and the APF entry
  stay as they are.
- It does not run the allocation job. The libraries exist already.
- It does not touch the document root disk.
- It does not touch your procedure, your Parmlib member or your RAKF
  definitions. SMP never writes to #cmd("PARMLIB") or to a procedure library.
  What changed in the sample members is yours to compare and to carry over
  (@ug-upgrade-review).
- It leaves no way back. The install job accepts the new level, and the old
  FMID is deleted. SMP cannot restore the old modules.

The captures in this chapter come from the upgrade from 4.5.0 to 4.5.1 on a
TK5 system. Every step of the install job ended with return code 0000 except
the APPLY, which ended with return code 0004 and
#cmd("HMA2461 SYSMOD THTP450 NOT FOUND ON SMPSCDS LIBRARY"). The package of
4.5.1 was built from the source of the release branch, with the version in the
project file changed from #cmd("4.5.1-dev") to #cmd("4.5.1"). The build stamp
in the start-up banner is therefore not the one of the published package
(@ug-upgrade-verify). The file names in your package carry the number of your
release.

== One Release at a Time <ug-upgrade-hops>

#idx("FMID")
#idx("THTP451")
Go from release to release. Do not skip one. The reason is in the way SMP
decides who owns a module.

The FMID of a release is #cmd("THTP") and the three digits of its version.
For a minor release, the package build derives the FMID and the predecessor
from the version number. 4.5.1 is a patch release, which the derivation has no
id for, so the project file of 4.5.1 names both: #cmd("THTP451"), deleting
#cmd("THTP450"). A release deletes only the FMID of the release before it.

#tab(caption: [Releases, FMIDs and what each one deletes])[
  #table(columns: (1in, 1.2in, 1fr),
    [Release], [FMID], [Deletes],
    [4.1.0], [#cmd("THTP410")], [#cmd("THTP400")],
    [4.2.0], [#cmd("THTP420")], [#cmd("THTP410")],
    [4.3.0], [#cmd("THTP430")], [#cmd("THTP420")],
    [4.4.0], [#cmd("THTP440")], [#cmd("THTP430")],
    [4.5.0], [#cmd("THTP450")], [#cmd("THTP440")],
    [4.5.1], [#cmd("THTP451")], [#cmd("THTP450")],
  )
]

SMP keys the ownership of a module on its name. The SYSMOD of 4.5.1 can take
over #cmd("HTTPD") from #cmd("THTP450") because it deletes #cmd("THTP450"). It
cannot take it over from #cmd("THTP440") or from an older FMID. By the same
rule, if the system is at 4.4.0 and you install 4.5.1, the SYSMOD does not own
the module, and the install copies nothing. Every step still ends with return
code 0. The inventory then says that 4.5.1 is installed, and the library holds
the modules of 4.4.0. This rule is SMP's and holds for every pair of releases.
The case of 4.4.0 and 4.5.1 was run on a TK5 system at 4.4.0: every step of the
install job of 4.5.1 ended with return code 0000, the #cmd("APPLY") had no
#cmd("HMA2380") line, and the banner of the started server still showed 4.4.0.
The library kept the modules of 4.4.0.

A skipped release has a sign in the output. The #cmd("APPLY") step has no
#cmd("HMA2240") lines (a deleted load module) and no #cmd("HMA2380") lines (a
copied load module). A good upgrade has five of each. Look for them in every
upgrade (@ug-upgrade-result). The lines #cmd("NOT SEL") are no sign: a good
upgrade has them too.

Each id is spent once. The inventory of the skipped install is not a state you
can repeat. This book does not show a measured repair for it. Check the level
first (@ug-upgrade-before), so that the case does not arise.

The accepted copy has its own zone, the ACDS, and the ACCEPT step deletes
there on its own. An upgrade is complete when both zones have moved
(@ug-upgrade-verify).

A system at 4.0.x needs the packages of 4.1.0, 4.2.0, 4.3.0, 4.4.0 and 4.5.0
before the package of 4.5.1. The hop from 4.0.x to 4.1.0 is different from the
others, and @ug-upgrade-40x describes it.

== Before You Start <ug-upgrade-before>

#idx("level")
Goal: you know the present level, you have the packages you need, and you have
kept what the upgrade replaces.

*The present level.* Ask SMP for the level of the load module #cmd("HTTPD")
and for the two SYSMODs. The job is the procedure #cmd("SMPAPP") with these
control statements. Name the FMID of your present level and the FMID of the
release you install:

#screen(```
//LISTCDS  EXEC SMPAPP
//SMPCNTL  DD  *
 RESETRC .
 LIST CDS SYSMOD(THTP450) .
 LIST CDS LMOD(HTTPD) .
 LIST CDS SYSMOD(THTP451) .
/*
```)

The output of this job, on the system before the upgrade:

#screen(raw(read("../ex/ug-upgrade/before-smp-list.txt")))

Read three facts. The line #cmd("LASTUPD = THTP450") under the load module
#cmd("HTTPD") is the present level: the module in the library comes from that
FMID. The SYSMOD #cmd("THTP450") has the status #cmd("REC  APP  ACC") and the
line #cmd("DELETE VER(001) = THTP440"), the one it replaced. And the new
SYSMOD #cmd("THTP451") is not known: the answer is the list of selected
entries that were not found or not eligible, and the step ends with return
code 04. That is the answer you want before the upgrade.

If #cmd("LASTUPD") does not name the predecessor of the release you are about
to install (@ug-upgrade-hops), install the missing releases first.

*The library members.* The library before the upgrade, listed with the
utility IEHLIST. The list is the reference for the comparison after the
upgrade:

#screen(raw(read("../ex/ug-upgrade/before-linklib-list.txt")))

The five members are the five load modules of the package. The column
#cmd("TTRC") is the place of the member in the library.

*Copies.* The install job scratches #cmd("HTTPD.SAMPLIB") and receives it
again (@ug-upgrade-job). If you edited a member there, copy the member out
before you submit the job. Your procedure and your Parmlib member belong in
#cmd("PROCLIB") and #cmd("PARMLIB"), where SMP does not look.

A way back is yours to build, outside SMP. Copy #cmd("HTTPD.LINKLIB") to a data
set of another name before the upgrade, with the utility you prefer. In an
emergency a #cmd("STEPLIB") that names the copy starts the old modules. The
copy needs an APF entry of its own for that. This book shows no session of it.

*The STEPLIB.* Look at the #cmd("STEPLIB") of your procedure. If it names a
development library before #cmd("HTTPD.LINKLIB"), that library wins, and the
server keeps running the module of the development build after the upgrade.
The system takes the first library of the concatenation that has the module.
"The Started Task and the Package" in the _HTTPD Reference_, section "The
STEPLIB", has the rule.

== Upgrading by One Release <ug-upgrade-hop>

An upgrade has the same tasks as an installation, without the allocation.

=== Stop the Server <ug-upgrade-stop>

#idx("stop")
Goal: nothing has #cmd("HTTPD.LINKLIB") open. The started task loads from the
library, and the install job writes into it. A job that cannot get the library
waits (@ug-install describes the wait).

Stop the server with the console command #cmd("P HTTPD"):

#screen(raw(read("../ex/ug-upgrade/stop-httpd.txt")))

The message #cmd("HTTPD098I") starts the stop. The threads end one by one
(#cmd("HTTPD060I")), and #cmd("HTTPD416I") prints the counters of the run. The
message #cmd("HTTPD099I") says the shutdown is complete. The line
#cmd("$HASP395 HTTPD ENDED") says that the address space is gone. Wait for it.
There is no #cmd("F HTTPD,SHUTDOWN"), although the comments in the sample
procedure list it ("Console Commands" in the _HTTPD Reference_).

=== Send the XMIT Files <ug-upgrade-upload>

Goal: the two XMIT files of the new release are on the system as data sets
with fixed records of 80 bytes, sent in binary. The session is the same as for
an installation:

#screen(raw(read("../ex/ug-upgrade/upload-ftp.txt")))

The names of the data sets are yours. The install job takes them in the next
task. @ug-install explains the upload, and what to do with a text transfer.
Use FTP or another route that does not need the HTTP server. If mvsMF is your
upload path, it is stopped now.

=== Edit and Submit the Install Job <ug-upgrade-job>

#idx("install job")
Goal: the install job names your two data sets. Do not run the allocation job.
The package README says to run it before the install job. For an upgrade you
skip that step: the libraries exist and hold the modules that the SYSMOD
replaces.

Change the two #cmd("RECEIVE INDSN") lines of the install job and nothing
else. The listing is the difference between the original and the edited job:

#screen(raw(read("../ex/ug-upgrade/inst-jcl-edit.txt")))

Submit the job as in @ug-install. The job takes about a minute.

== Reading the Result <ug-upgrade-result>

#idx("return code")
Goal: know whether the upgrade did its work. The console shows that the job
started and that it ended. It does not show the steps:

#screen(raw(read("../ex/ug-upgrade/inst-console.txt")))

The step table is in the job log, with the time of each step:

#screen(raw(read("../ex/ug-upgrade/inst-joblog.txt")))

The step #cmd("APPLY") ends with 0004. That is the normal result of an
upgrade. The next paragraphs say why. Every other step ends with 0000.

*DELOLD.* The step makes the two RECEIVE targets absent. Both are data sets of
the previous run, so the capture looks different from the installation:

#screen(raw(read("../ex/ug-upgrade/inst-delold.txt")))

The data set #cmd("HTTPD.HTTPLOAD") is not in the catalog. The previous
install job scratched it in its last step, and the delete ends with condition
code 8. The data set #cmd("HTTPD.SAMPLIB") exists, and the message
#cmd("IDC0550I") says it is deleted. Your copies of the old members are gone
with it. The statement #cmd("SET MAXCC=0") makes the step end with 0.

*RECV1 and RECV2.* TSO RECEIVE unpacks the two XMIT files:

#screen(raw(read("../ex/ug-upgrade/inst-recv-load.txt")))
#screen(raw(read("../ex/ug-upgrade/inst-recv-samplib.txt")))

The line to find is #cmd("Receive into ... successful"). The name after
#cmd("Receiving") is the name the XMIT had where it was made. It is not the
name of your data set.

*RECV.* SMP stores the SYSMOD. The output repeats it and ends with
#cmd("HMA3930"):

#screen(raw(read("../ex/ug-upgrade/inst-smp-receive.txt")))

The second line of the SYSMOD is the one that makes the upgrade:
#cmd("++VER(Z038) DELETE(THTP450)"). It names the predecessor.

*APPLYCHK.* A dry run. It changes nothing. The summary names the predecessor
as deleted:

#screen(raw(read("../ex/ug-upgrade/inst-smp-applycheck.txt")))

The return code is 00. The lines #cmd("THTP450 DELETED") show that SMP knows
the predecessor and will delete it. The step has no #cmd("HMA2240") and no
#cmd("HMA2380"), because a check does not do the work.

*APPLY.* This step does the work:

#screen(raw(read("../ex/ug-upgrade/inst-smp-apply.txt")))

Read these lines in order:

+ #cmd("HMA2240 SUCCESSFULLY DELETED LMOD ... ON LINKLIB LIBRARY"), five
  times. SMP deletes the load modules of the old FMID from
  #cmd("HTTPD.LINKLIB"). This is the delete that the SYSMOD asked for.
+ #cmd("HMA4180 INLINE JCLIN PROCESSING SUCCESSFUL"). SMP has read the JCLIN
  of the new SYSMOD.
+ #cmd("HMA2380 COPY SUCCESSFUL - MOD=... - LIBRARY=LINKLIB"), five times. SMP
  copies the new load modules in. This is the line that separates a real
  upgrade from a silent one. Count it.
+ #cmd("HMA2270 APPLY PROCESSING SUCCESSFULLY COMPLETED FOR SYSMOD THTP451").
+ #cmd("HMA2461 SYSMOD THTP450 NOT FOUND ON SMPSCDS LIBRARY"), and the return
  code 04. SMP looks for a backup entry of the deleted FMID, and a deleted FMID
  has none. The message comes after the success message and does not undo it.

The page at the end, #cmd("DELETED FUNCTION REPORT"), shows #cmd("THTP451") as
the function that deleted #cmd("THTP450"). The report is the second proof that
the predecessor is gone.

The copy runs through the utility IEBCOPY. Its listing from the same step:

#screen(raw(read("../ex/ug-upgrade/inst-apply-iebcopy.txt")))

The marker #cmd("R") after each member name is the replace option: the new
module replaces the member of the same name. A load module with the attribute
AC(1) keeps it, because SMP copies the module and does not link it again.

*ACCEPT.* The step runs although the APPLY ended with 0004. The install job
lets it run for a code of 4 or less. The step does the same work on the
distribution library:

#screen(raw(read("../ex/ug-upgrade/inst-smp-accept.txt")))

Five #cmd("HMA2240") lines delete the old modules from #cmd("AHTTPLOD"), and
five #cmd("HMA2380") lines copy the new ones in. The step ends with 00.
The gate in the job lets the #cmd("ACCEPT") run for an #cmd("APPLY") code of 4
or less. A code of 8 or more stops it.

*CLEANUP.* The last step scratches the staging library:

#screen(raw(read("../ex/ug-upgrade/inst-cleanup.txt")))

The return codes mean what they meant in @ug-install. The one difference is
the 0004 of the #cmd("APPLY"). A 0004 in another step, or in a job that has no
delete, needs a look at the messages. And a return code of 0 in every step does
not show that the work happened. Count the #cmd("HMA2240") and #cmd("HMA2380")
lines, and then verify (@ug-upgrade-verify).

== Verifying the Upgrade <ug-upgrade-verify>

#idx("verify")
Goal: the inventory and the library both show the new level.

*The SMP record.* Run the list job again. The control statements are the same
as before the upgrade. The output:

#screen(raw(read("../ex/ug-upgrade/after-smp-list.txt")))

Four facts changed. The old SYSMOD #cmd("THTP450") has lost its status, and it
shows only the line #cmd("DELBY = THTP451"): the FMID that deleted it. The new
SYSMOD #cmd("THTP451") has the status #cmd("REC  APP  ACC"), the line
#cmd("DELETE VER(001) = THTP450") and the five modules. And the load module
#cmd("HTTPD") has #cmd("LASTUPD = THTP451"). The old FMID stays in the
inventory as a mark and is never used again.

*The library.* The IEHLIST listing after the upgrade:

#screen(raw(read("../ex/ug-upgrade/after-linklib-list.txt")))

Compare it with the listing before. The five names are the same. The
#cmd("TTRC") and the user data of each member are not. The members are new
copies. If the listing is the same as before, the copy did not happen.

*The start.* Start the server with the old procedure and the old Parmlib
member:

#screen(raw(read("../ex/ug-upgrade/start-new.txt")))

The message #cmd("HTTPD000I") carries the version of the build, and
#cmd("HTTPD001I") says that the server is ready. The three #cmd("HTTPD036I")
lines are the routes of the member. If the version is not the one you
installed, look at the #cmd("STEPLIB") (@ug-upgrade-before). Send a request to
the server, as in @ug-first.

Two things in the capture are new in 4.5.1. The stamp in #cmd("HTTPD000I") is
shown as #cmd("nnnnnnn"): the package of the capture was built from the source
of the release branch with the version changed from #cmd("4.5.1-dev") to
#cmd("4.5.1"), and a start of the published package was not measured
for this book. The line #cmd("RAKF0004 INVALID ATTEMPT TO ACCESS SYSTEM:  USER:HTTPPRB")
comes from the test of the security system at the start. It is expected on TK5
(RAKF 1.2.x) and MVS/CE (RAKF 2.0.0), and "Starting HTTPD" in @ug-install explains it. If the
line #cmd("HTTPD042W") appears instead, the upgrade did not cause it: look at
the tables of RAKF (@ug-security-open).

== What the SMP Upgrade Deletes <ug-upgrade-deletes>

#idx("predecessor")
The delete covers the load modules of the predecessor and nothing more.

- *In the target library.* The five load modules #cmd("HTTPD"),
  #cmd("HTTPDSRV"), #cmd("HTTPDM"), #cmd("HTTPDMTT") and #cmd("ABEND0C1"),
  from #cmd("HTTPD.LINKLIB"), then the new ones are copied in (the
  #cmd("HMA2240") and #cmd("HMA2380") lines of the APPLY).
- *In the distribution library.* The same five, from #cmd("HTTPD.AHTTPLOD")
  (the same messages in the ACCEPT).
- *In the inventory.* The predecessor #cmd("THTP450") becomes a mark with the
  line #cmd("DELBY"). The listing shows it for the CDS, and the summary of the
  ACCEPT shows the same for the ACDS. The load module entries move to the new
  FMID.

The upgrade does not delete the procedure, the Parmlib member, the webroot
disk, the RAKF entries and the mounts of UFSD. The sample library is not
SMP's: the first step of the job scratches it, and the package brings the new
one. The capture shows a library of only five members, so this
book does not show what SMP does with a foreign member in
#cmd("HTTPD.LINKLIB"). The library belongs to SMP. Keep other modules, such as
mvsMF, in a library of their own.

*What was verified.* The FMID rule comes from the project file. The entry
#cmd("[smp] prefix = \"THTP\"") makes the build derive the FMID and the
predecessor from the version, and the project file of 4.5.1 names
#cmd("fmid = \"THTP451\"") and #cmd("delete = [\"THTP450\"]") itself, because
4.5.1 is a patch release. A package build of 4.5.1 produced
#cmd("++FUNCTION(THTP451)") and #cmd("++VER(Z038) DELETE(THTP450)"). The other
rows of the table in @ug-upgrade-hops come from the project file and from the
measured installs of the releases. The step output in this chapter is from the
upgrade 4.5.0 to 4.5.1, where #cmd("THTP451") deleted #cmd("THTP450"):
#cmd("THTP451") is a #cmd("FUNCTION") with the line
#cmd("DELETE VER(001) = THTP450"), #cmd("THTP450") shows
#cmd("DELBY = THTP451") and no modules, and #cmd("HTTPD.LINKLIB") and
#cmd("HTTPD.AHTTPLOD") each hold the five members of the package. Earlier
upgrades, from 4.3.0 to 4.4.0 and from 4.4.0 to 4.5.0, had the same shape.

== Upgrading from HTTPD 4.0.x <ug-upgrade-40x>

#idx("4.0.x")
The libraries of 4.0.x carry the version in their names. One of them is
#cmd("HTTPD.V4R0M1.LINKLIB"). From 4.1.0 on, the names carry no version. The hop
from 4.0.x to 4.1.0 is the one where the names change, and so it is the one
hop that needs the allocation job. Every hop after it does not.

+ Take the package of 4.1.0. Run its allocation job. It creates
  #cmd("HTTPD.LINKLIB") and #cmd("HTTPD.AHTTPLOD") (@ug-install).
+ Make the new #cmd("HTTPD.LINKLIB") available to the started task. Put it
  into the #cmd("STEPLIB") of the procedure in place of the old library, and
  make it authorized, with an APF entry or through RAKF ("Authority" in
  @ug-install). The volume matters: the APF entry names it.
+ Send the XMIT files, edit the install job and submit it, as in
  @ug-upgrade-hop. The #cmd("APPLY") step deletes #cmd("THTP400") from the
  new, empty library without a complaint, copies the five modules in, and ends
  with 0004. The #cmd("ACCEPT") step ends with 0000.
+ Install 4.2.0, 4.3.0, 4.4.0, 4.5.0 and 4.5.1 in turn, each as in
  @ug-upgrade-hop. These hops skip the allocation job.

The install of 4.1.0 leaves the old libraries as they were. After the
crossing, check these points:

- *mvsMF.* The old library held the module #cmd("MVSMF"). The new
  #cmd("HTTPD.LINKLIB") does not, and it should not. Install mvsMF from its own
  package into a library of its own, and put the library into the
  #cmd("STEPLIB") behind #cmd("HTTPD.LINKLIB") or into the link list. Without
  it, a request for a route of mvsMF gets an error response (@ug-install).
- *The old libraries.* The data sets #cmd("HTTPD.V4R0M*.*") stay on the
  volume. Scratch them yourself, when the new server runs and nothing names
  them any more. List the data sets first, and check the #cmd("STEPLIB"), the
  APF list and the link list. Look at the names before you scratch: an old
  webroot data set may carry a name of the same pattern. The document root disk
  is not a library of the product. It holds your content and the UFSD mount
  names it. Do not scratch it with them.
- *The procedure.* Compare it with the sample (@ug-upgrade-review).

*The JES2 DDs.* The procedure of 4.0.0 had no #cmd("HASPCKPT") and
#cmd("HASPACE1") statements. mvsMF 1.3.0 and earlier open the two data sets by
their DD names in the task of the server, and without the statements they
answer a job list with status 500 and a request for one job with status 404.
The sample procedure of 4.0.1 has the statements back. From 4.5.0 on, the
sample procedure has them as comment lines, because mvsMF 1.4.0 and later do
not need them. A procedure that you copied from 4.0.0 needs them active when you
run mvsMF 1.3.0 or earlier or a JES2 with a prefix other than #cmd("SYS1")
(@ug-install).

*A clean cut instead.* If you have all the packages, but would rather not
install five releases in turn, you can remove HTTPD with @ug-remove and then
install the current release with @ug-install. The removal job frees the
FMIDs and scratches the libraries, and the installation is the one for a new
system. Keep a copy of your procedure, your Parmlib member and your webroot disk
first, because @ug-remove treats the parts that SMP never owned in a step of its
own. This way has not been run for this book as a whole.

== Upgrading from HTTPD 3.x <ug-upgrade-3x>

#idx("3.x")
#idx("migration")
HTTPD 3.x was not an SMP install, and the step from 3.x to 4.x is a
migration and not an upgrade. The new server is a new install (@ug-install).
It has a new procedure and a new configuration. This section lists what a
reader of the old documentation needs to know.

The data sets of the new install have the names of @ug-install. If a data set
of the old installation has one of those names, the allocation job cannot
create it. Rename the old data set first. This book does not show the
rename.

*The configuration.* The Lua configuration file is gone. The server reads the
Parmlib member that the DD name #cmd("HTTPPRM") of the procedure names, with
lines of the form #cmd("KEYWORD=VALUE"). Start from the sample member of the
package (#cmd("HTTPPRM0") in #cmd("HTTPD.SAMPLIB")) and move your settings
into it. "The HTTPPRM Keywords" in the _HTTPD Reference_ lists every keyword.
@ug-config explains the file.

*The routes.* A program route is a line #cmd("MOD=PROGRAM /path") with an
optional #cmd("AUTH=mode"). A route without #cmd("AUTH=") is public. Write the
#cmd("AUTH=") on every route (@ug-routes, @ug-security). The 3.x keyword
#cmd("LOGIN") has no place any more. HTTPD ends the start with
#cmd("HTTPD420E") and CC 0008 if the value would have required a login
(#cmd("HTTPD048E")), because the routes would be open without it. An empty
value, or the value #cmd("NONE"), gives #cmd("HTTPD048W") and the start goes on.
HTTPD does not turn the value into route policies: change the routes ("Routes
and Authentication Policy" in the _HTTPD Reference_).

*The parameter CONFIG.* A procedure that passes #cmd("PARM='CONFIG=...'") still
starts. HTTPD ignores the parameter and writes #cmd("HTTPD024W") and
#cmd("HTTPD024I"), which says to use #cmd("S HTTPD,M=MEMBER") to choose a
different member. Remove the parameter from the procedure. The command
#cmd("F HTTPD,D CONFIG") shows with #cmd("HTTPD133I") which member HTTPD read.

*Static files.* The files are served from a UFS file system through UFSD, and
from nowhere else. A data set as document root is not supported. Put the files
into the webroot and have UFSD mount it (@ug-install, @ug-static).

*What is gone.* These features of 3.x are not in 4.x:

#tab(caption: [Features of HTTPD 3.x that are not in 4.x])[
  #table(columns: (1.7in, 1fr),
    [Feature], [What to use now],
    [The embedded FTP server], [The standalone FTPD, as a started task of its
      own.],
    [MQTT telemetry], [Nothing replaces it. The server writes SMF records.],
    [The Lua engine], [HTTPLUA, a module of its own project that you add as a
      route.],
    [The demo modules], [Nothing. The module ABEND0C1 is still in the package,
      and it is inert unless a route names it.],
    [A data set as document root], [A UFS file system, through UFSD.],
    [The in-memory statistics], [SMF records. The keyword #cmd("SMF") has the
      default #cmd("NONE"), so set it if you want records ("The HTTPPRM
      Keywords" and the SMF chapter of the Reference).],
    [HTTPJES2 and HTTPDSL], [The dataset and jobs APIs of mvsMF.],
  )
]

A route that names #cmd("HTTPJES2") or #cmd("HTTPDSL") loads nothing. The start
writes #cmd("HTTPD440W") for a module that is not found, and a request to the
route gets status 503 with #cmd("HTTPD908E") on the console. Delete the line.

*The DD names.* Keep the statements #cmd("HASPCKPT") and #cmd("HASPACE1") in
the procedure when you run mvsMF 1.3.0 or earlier. A module runs in the task of
the server, and these releases of mvsMF open the two data sets by DD name. From
mvsMF 1.4.0 on, the statements are optional: the jobs API allocates
#cmd("SYS1.HASPCKPT") and #cmd("SYS1.HASPACE") itself when the procedure has no
DD. They stay needed on a JES2 whose prefix is not #cmd("SYS1"). Leaving them
in is safe.

*What is new.* HTTP/1.1 with persistent connections and chunked transfer, a
strict parser, and routing by the extension of a file with #cmd("MOD=") (@ug-routes).

*The checklist.* In this order:

+ Install the 4.x package (@ug-install).
+ Copy the sample Parmlib member and the sample procedure, and edit them for
  your system. Remove #cmd("CONFIG=") from the procedure.
+ Start UFSD, and mount the webroot.
+ Move your routes into the member, each with an #cmd("AUTH=").
+ Install FTPD as a separate task, if you used the FTP server.
+ Decide on #cmd("SMF").
+ Start HTTPD and test with a client (@ug-first).

== After the Upgrade: Review Your Own Files <ug-upgrade-review>

#idx("sample members")
The upgrade replaces the sample library and leaves your copies alone. Nothing
in an old member is wrong because a newer sample exists, but a newer sample
can contain a thing that your member lacks. Compare your member with the new
new sample #cmd("HTTPD.SAMPLIB(HTTPPRM0)"), and your
procedure with #cmd("HTTPD.SAMPLIB(HTTPD)"), by eye or with the compare tool
of your system. These are the changes that a reader of an older release needs
to look for. The first two matter for the safety of the system.

- *The console route of mvsMF.* Since 4.4.0 the sample member has the route
  #cmd("MOD=MVSMF /zosmf/restconsoles/*") with
  #cmd("AUTH=TOKEN RES=FACILITY:MVSMF.CONSOLE"), before the route
  #cmd("/zosmf/*"). The console service issues operator commands for every
  caller who can log in, and mvsMF up to 1.3.0 checks nothing itself. From
  mvsMF 1.4.0 on it checks READ on the same profile too and refuses every user
  until the profile exists; the route stays as a first layer. The route must come
  before the catch-all, because the first match wins. A member that has only
  the line #cmd("/zosmf/*") has no check on the console service. Add the route,
  and define the profile in RAKF (@ug-security). Without a profile the check
  permits everybody under RAKF, and #cmd("HTTPD425W") says so at the start.
- *The login checks of 4.5.1.* Nothing in your files has to change. The server
  now compares the user that the security system returns with the userid that
  was typed, and refuses the login on a mismatch. It refuses every login of the
  started-task account. At the start it tests the security system once with a
  logon that must fail. On TK5 and MVS/CE this writes
  #cmd("RAKF0004 INVALID ATTEMPT TO ACCESS SYSTEM:  USER:HTTPPRB") to the
  console, and the line is expected. On #cmd("HTTPD042W") the server refuses
  every login until the security software runs correctly and HTTPD has been
  restarted, while routes without a login still answer (@ug-security-open).
- *The AUTH values.* A route without #cmd("AUTH=") is public. Open the member
  and read every #cmd("MOD") and #cmd("LOC") line for it.
- *A route whose profile is missing.* From 4.5.0 the keyword
  #cmd("RES_NOPROFILE=DENY") makes a route with #cmd("RES=") refuse every
  request while its profile does not exist, instead of letting every user who
  is logged in pass. The sample member has it as a comment line, and without it
  nothing changes. Turn it on when the start prints no #cmd("HTTPD425W")
  (@ug-security-noprofile).
- *Resident modules.* From 4.4.0 the keyword #cmd("RESIDENT") has the default
  #cmd("YES"): HTTPD loads each module of a route once at the start and holds
  it until the end. A new level of a module then takes effect at the next
  restart. This matters when you install a new mvsMF. With #cmd("RESIDENT=NO")
  every request fetches the module. A module that is neither reentrant nor
  serially reusable is not held, and the start says so with #cmd("HTTPD441W").
- *The procedure.* It names the library in the #cmd("STEPLIB") and the Parmlib
  member in the #cmd("HTTPPRM") statement. From 4.0.1 on it had the two JES2
  data sets, and from 4.5.0 on it has them as comment lines. Your own active
  statements keep working, so you need not change them. If you run mvsMF 1.4.0
  or later on a JES2 with the prefix #cmd("SYS1"), you may comment them out. If
  you keep them, check that #cmd("UNIT") and #cmd("VOL=SER") match your own JES2
  procedure.

"The HTTPPRM Keywords" in the _HTTPD Reference_ has a section on the keywords
that are no longer valid. A member from an earlier release starts, except for a
retired #cmd("LOGIN") that required a login.

== Problems at Upgrade <ug-upgrade-problems>

#idx("problems")
@ug-trouble has the general table. These are the ones that belong to an
upgrade.

#tab(caption: [Problems at upgrade])[
  #table(columns: (1.9in, 1fr),
    [What you see], [What it is and what to do],
    [The #cmd("APPLY") step ends with 0004 and the #cmd("ACCEPT") ran.], [The
      normal result. The deleted FMID has no backup entry
      (#cmd("HMA2461")).],
    [Every step ends with 0000, but the #cmd("APPLY") has no #cmd("HMA2240") and
      no #cmd("HMA2380").], [A release was skipped. The SYSMOD does not own
      the modules and copied nothing. The library still holds the old modules.
      List the level (@ug-upgrade-before) and do not go on.],
    [The job waits, and the console shows #cmd("IEF861I") and #cmd("IEF099I").],
    [HTTPD still runs and has the library open. Stop it with #cmd("P HTTPD")
      and wait for #cmd("$HASP395 HTTPD ENDED").],
    [A RECEIVE step fails.], [The upload was not binary, or the record format
      is wrong. Send the file again (@ug-install).],
    [The version in #cmd("HTTPD000I") is the old one.], [The
      #cmd("STEPLIB") names another library first, or an old copy of the
      module is in the link list. Check both.],
    [The start ends with #cmd("HTTPD420E") and CC 0008.], [A route lost its
      policy: a retired #cmd("LOGIN") (#cmd("HTTPD048E")), an unknown
      #cmd("AUTH=") value or a route that cannot be registered. Read all
      the messages before it.],
    [#cmd("RAKF0004 ... USER:HTTPPRB") at the start.], [Expected on TK5 and
      MVS/CE since 4.5.1: the test of the security system. No user
      #cmd("HTTPPRB") needs to exist.],
    [#cmd("HTTPD042W") at the start, and every login is refused.], [The
      security software accepts any user ID. On RAKF 1.2.x the user table is
      missing or was rejected at the IPL. Correct the tables, IPL, and restart
      HTTPD (@ug-security-open). Until then the server refuses every login.],
    [#cmd("HTTPD440W") at the start.], [A route names a module that HTTPD does
      not find in the #cmd("STEPLIB") or the link list. After a move from
      4.0.x, mvsMF is the likely one.],
    [#cmd("HTTPD002I") says #cmd("AUTHORIZED BY SVC"), and you expect
      #cmd("AUTHORIZED BY LIBRARY"). Or #cmd("HTTPD012E") at the start.], [The
      library is not the one the APF entry names, for example the new library
      of a move from 4.0.x on another volume. HTTPD falls back to the SVC of
      RAKF. Without RAKF it ends with #cmd("HTTPD012E") ("Authority" in
      @ug-install).],
    [A job list of mvsMF answers 500. A request for one job or its spool
      answers 404 "job not found" although the job exists.], [mvsMF 1.3.0 or
      earlier, or a JES2 with a prefix other than #cmd("SYS1"), and the
      procedure lacks #cmd("HASPCKPT") and #cmd("HASPACE1"). Add the two
      statements (@ug-install).],
  )
]

To take HTTPD out of the system, follow @ug-remove.

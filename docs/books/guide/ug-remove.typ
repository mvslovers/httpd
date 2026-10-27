#import "../bookmaster/bookmaster.typ": *

= Removing HTTPD <ug-remove>

#idx("removal")
#idx("uninstall")
This chapter takes HTTPD off an MVS 3.8j system and frees its FMID. It covers
the SMP inventory, the load and distribution libraries, and the parts that SMP
never owned: the procedure, the Parmlib member, the RAKF entries and the
document root disk. The last section shows how to check that the work
happened.

If you want a newer release, you are in the wrong chapter. A release deletes
its predecessor when it is installed (@ug-upgrade). Running this chapter first
would scratch the libraries that the new release installs into.

#note[*Which release the sessions come from.* The sessions in this chapter were
measured on release 4.4.0 (the FMID #cmd("THTP440")) on a TK5 system. The FMID
of a release is #cmd("THTP") and the three digits of its version. HTTPD 4.5.0 is
#cmd("THTP450"), and it deletes #cmd("THTP440"); the rule is in the _HTTPD
Reference_, in "The Started Task and the Package", section "The FMID". The job
in this chapter is the 4.4.0 job. @ug-remove-45 tells what changes for 4.5.0.]

== What HTTPD Put on the System <ug-remove-what>

#tab(caption: [What release 4.4.0 installed])[
  #table(columns: (1.8in, 1fr),
    [Item], [Name],
    [FMID], [#cmd("THTP440")],
    [Load modules], [#cmd("HTTPD"), #cmd("HTTPDSRV"), #cmd("HTTPDM"),
      #cmd("HTTPDMTT"), #cmd("ABEND0C1")],
    [Target library], [#cmd("HTTPD.LINKLIB")],
    [Distribution library], [#cmd("HTTPD.AHTTPLOD")],
    [Sample library], [#cmd("HTTPD.SAMPLIB") (SMP does not own it)],
    [Document root disk], [#cmd("HTTPD.WEBROOT.UFS") (SMP does not own it)],
  )
]

The names of the data sets carry no version, so there is one HTTPD
installation on a system, whichever release put it there. The staging library
#cmd("HTTPD.HTTPLOAD") is not on the list. The last step of the install job
scratches it, but only if the ACCEPT ended with return code 0. If a failed
install left it, scratch it as well.

*Earlier releases left their FMIDs behind.* One release has one FMID, and an
id is spent once. Each release deletes its predecessor, and SMP keeps the id of
the deleted function in both zones as a mark. SMP lists the mark as
#cmd("TYPE") #cmd("FUNCTION") with #cmd("DELBY") naming the later id, and its
#cmd("LIST") of such an id ends with return code 0, not 4. A system that took
four upgrades carries four of these marks. A removal that names only the
current release and its predecessor leaves the older marks in the inventory for
good. The job in this chapter therefore names every FMID that HTTPD has ever
spent, from #cmd("THTP400") to #cmd("THTP440"). You need not know which
releases your system has seen.

== Step 1: Stop the Server <ug-remove-stop>

#idx("P HTTPD")
#idx("stop", "before removal")
Stop the started task from the console. The server writes its shutdown
messages and the statistics of the run, and the task ends.

#screen(raw(read("../ex/ug-remove/stop-httpd.txt")))

#cmd("HTTPD098I") starts the stop and #cmd("HTTPD099I") ends it. Between them
the server writes one line for the listener thread and one for each worker.
The messages are in the _HTTPD Reference_, in 'Messages', section "A Healthy
Start and Stop". #cmd("$HASP250 HTTPD IS PURGED") shows that JES2 has dropped
the task.

If you run a second HTTPD on another port, stop that one as well. It loads
from a library that step 4 scratches.

== Step 2: Cut the FMIDs out of the SMP Inventory <ug-remove-uclin>

#idx("UCLIN")
#idx("DEL SYSMOD")
#idx("SMP", "removal")
#idx("SMPAPP")
The job below edits the CDS (what is applied) and the ACDS (what is accepted).
It touches no library. It is long on purpose. This is the job as it ran on the
measured system. #cmd("SMPAPP") is the SMP procedure of TK5. If your system
names it differently, change the two #cmd("EXEC") statements.

#screen(raw(read("../ex/ug-remove/uclin-job.txt")))

The job has 35 #cmd("DEL") statements, 20 against the CDS and 15 against the
ACDS. In each zone they fall into these groups.

+ Five #cmd("DEL SYSMOD(THTP440) MOD(...)") statements. They take the five
  modules out of the entry of the FMID.
+ Five #cmd("DEL MOD(...)") statements. They remove the module entries.
+ Five #cmd("DEL LMOD(...)") statements, in the CDS only. They remove the load
  module entries.
+ Five #cmd("DEL SYSMOD(...)") statements, one for each id from
  #cmd("THTP440") down to #cmd("THTP400"). They remove the function entries,
  and with them the marks of the deleted releases.

Every #cmd("DEL") stands in a #cmd("UCLIN")/#cmd("ENDUCL") pair of its own,
and a #cmd("RESETRC .") follows every #cmd("ENDUCL ."). The reason is the way
SMP treats a #cmd("DEL") for an entry that the zone does not hold.

- The #cmd("DEL") is an error. SMP writes #cmd("HMA2532") (the entry does not
  exist) and #cmd("HMA2560") (update not complete), and the block ends with
  return code 8.
- An error stays in force for the rest of the step. Every later
  #cmd("UCLIN") of the same step is answered with #cmd("HMA2293") (control
  statement ignored due to previous error), and its #cmd("DEL") statements are
  discarded without a trace.
- #cmd("RESETRC .") after each block lifts the error. A new #cmd("EXEC") step
  starts clean as well.

The job needs this because it names every FMID that HTTPD has spent, and no
system has seen all of them.

=== Reading the return codes <ug-remove-rc>

On the measured system, the job log says this.

#screen(raw(read("../ex/ug-remove/uclin-joblog.txt")))

The #cmd("UCLIN") step ends with return code 8 and the #cmd("LIST") step with
4. Both are the expected result on this system. The system held #cmd("THTP440"),
#cmd("THTP430") and the mark of #cmd("THTP420"), but it had never seen
#cmd("THTP410") or #cmd("THTP400"). The next screen counts the messages in the
SMP output of the #cmd("UCLIN") step.

#screen(raw(read("../ex/ug-remove/uclin-hma-summary.txt")))

Of the 35 statements, 31 ended with #cmd("HMA2550") (update complete). Four
ended with #cmd("HMA2532") and #cmd("HMA2560"): #cmd("THTP410") and
#cmd("THTP400"), in each of the two zones. There is no #cmd("HMA2293"), because
each block has its own #cmd("RESETRC .").

A return code of 8 here does not say that the removal failed, and a return
code of 0 would not say that it worked. The #cmd("LIST") of the next step
tells. If a #cmd("DEL") that you expected to work shows an error other than
#cmd("HMA2532"), read the complete SMP output of the #cmd("UCLIN") step. It
holds the messages between the statements.

== Step 3: Read the LIST <ug-remove-list>

#idx("LIST")
#idx("SMP", "LIST")
The second step of the job lists the five ids in both zones. This listing is
the result of the removal, not the return code of the job. This is the output
on the measured system.

#screen(raw(read("../ex/ug-remove/uclin-smp-list.txt")))

Each of the ten #cmd("LIST") statements must answer with the stanza "THE
FOLLOWING SELECTED ENTRIES WERE NOT FOUND OR WERE NOT ELIGIBLE FOR
PROCESSING", naming the id, and the step ends with return code 4. Return code
4 and an empty list mean that the FMID is free. Read all ten. The CDS and the
ACDS are separate inventories, and an id that is gone from one and present in
the other is not free.

A stanza that lists #cmd("TYPE") #cmd("FUNCTION") with #cmd("DELBY") is an id
that is still there. SMP answers it with return code 0, so it does not lift the
return code of the step above the 4 that you expect. This is why you read the
stanzas and not the return code of the step.

== Step 4: Scratch the Libraries <ug-remove-scratch>

#idx("IDCAMS")
#idx("scratch")
#idx("HTTPD.LINKLIB", "removal")
#idx("HTTPD.AHTTPLOD", "removal")
#note[*List the members first.* #cmd("HTTPD.LINKLIB") is the STEPLIB of the
started task, and it holds whatever was copied into it. The package puts five
modules there. A server module of another product, for example #cmd("MVSMF"),
can be in the same library. SMP does not know such a module, and the scratch
takes it with it. List the members (ISPF 3.4 or IEHLIST) and install again what
belongs to something else.]

The UCLIN job edited the inventory only. Both libraries are still catalogued,
and the allocation job of a later install would fail on them. This job scratches
them.

#screen(raw(read("../ex/ug-remove/scratch-job.txt")))

The job log shows that the step ran.

#screen(raw(read("../ex/ug-remove/scratch-joblog.txt")))

The return code says that the job ran. IDCAMS says what it did.

#screen(raw(read("../ex/ug-remove/scratch-idcams.txt")))

#cmd("IDC0550I") names the entry that was deleted, once for each library.
#cmd("HTTPD.SAMPLIB") is not in this job. The next install scratches it by
itself (the step #cmd("DELOLD") of the install job). Scratch it too if you want
a clean system, once you have copied what you want out of it.

If you put the library into the APF list (the entry in
#cmd("SYS1.PARMLIB(IEAAPF00)")), remove the entry for #cmd("HTTPD.LINKLIB")
too.

== Step 5: The Parts that SMP Never Owned <ug-remove-rest>

#idx("removal", "procedure")
#idx("removal", "RAKF")
#idx("removal", "webroot")
The UCLIN job and the scratch job do not touch these. They are yours.

*The procedure.* Delete the member #cmd("HTTPD") from the procedure library
that you copied it to, for example #cmd("SYS2.PROCLIB(HTTPD)").

*The Parmlib member.* Delete the member that the procedure names in its symbol
#cmd("M") (#cmd("HTTPPRM0") in the sample) from the data set that its symbol
#cmd("D") names. The _HTTPD Reference_ shows the procedure in "The Started Task
and the Package", section "The Procedure". Keep a copy if you want to look at
your routes again later.

*The RAKF entries.* The user of the started task (#cmd("HTTPD") in the group
#cmd("USER") in the sample procedure) has a line in
#cmd("SYS1.SECURE.CNTL(USERS)"). The routes with #cmd("RES=") can have
profiles in #cmd("SYS1.SECURE.CNTL(PROFILES)"). @ug-security shows the
definitions that HTTPD needs. Take care in this step.

- Remove the line of the HTTPD user from #cmd("USERS") only if no other
  product uses the user. Remove a profile only if no other product uses it. A
  profile of a route can belong to a server module of another product, for
  example mvsMF.
- Reload the tables with the RAKF started tasks, #cmd("S RAKFPROF") for the
  profiles and #cmd("S RAKFUSER") for the users.

*The document root disk.* The mount of #cmd("HTTPD.WEBROOT.UFS") belongs to
UFSD, not to HTTPD. Remove the #cmd("MOUNT") statement that names it from the
Parmlib member of UFSD. Unmount the disk or stop UFSD before you scratch the
data set (see the documentation of UFSD). Then scratch it with IDCAMS, as you
scratched the libraries.

#screen(```
  DELETE HTTPD.WEBROOT.UFS NONVSAM SCRATCH PURGE
```)

No SMP job removes the document root disk. The files that you put into it are
in it. Copy out what you want before you scratch it.

== Step 6: Check That It Is Gone <ug-remove-verify>

#idx("LISTCAT")
#idx("verify", "removal")
A return code says that a job ran. It does not say that the work happened.
Check the result in two places.

*SMP.* The #cmd("LIST") of step 3 is the check for the inventory. You can run
it again at any time. On a clean system every stanza answers "not found".

*The catalog.* List what is left under the high-level qualifier.

#screen(```
  LISTCAT LEVEL(HTTPD)
```)

This is the answer on the measured system, after the scratch job.

#screen(raw(read("../ex/ug-remove/after-listcat.txt")))

Neither #cmd("HTTPD.LINKLIB") nor #cmd("HTTPD.AHTTPLOD") is in the list. The
list still shows ten entries. Only #cmd("HTTPD.SAMPLIB") is a data set of the
package, and step 4 left it in place on purpose. The other nine are not data
sets that the package creates, and this chapter does not touch them. On your
system, check each remaining name before you scratch it. Expect
#cmd("HTTPD.WEBROOT.UFS") and #cmd("HTTPD.SAMPLIB") in the list until you
scratch them yourself.

== Running the Job Twice <ug-remove-again>

The job can run again. On a system where no id is left, the #cmd("UCLIN") step
ends with return code 8 for the missing entries and writes no #cmd("HMA2550"),
and the #cmd("LIST") gives the same answer. After the removal no zone holds the
id, so the same release can be installed again.

== What Changes for HTTPD 4.5.0 <ug-remove-45>

#idx("THTP450")
HTTPD 4.5.0 has the FMID #cmd("THTP450") and deletes #cmd("THTP440"). The job
above is the 4.4.0 job. For 4.5.0, make these changes in both zones.

- Add one block for #cmd("THTP450") to each zone. It has four lines:
  #cmd("UCLIN CDS .") (#cmd("UCLIN ACDS .") in the ACDS),
  #cmd("DEL SYSMOD(THTP450) ."), #cmd("ENDUCL .") and #cmd("RESETRC .").
- In the five #cmd("DEL SYSMOD(THTP440) MOD(...)") blocks, name #cmd("THTP450")
  in place of #cmd("THTP440"). The modules of a 4.5.0 system belong to the
  newest FMID. The block #cmd("DEL SYSMOD(THTP440) .") stays.
- Add #cmd("LIST CDS SYSMOD(THTP450) .") and
  #cmd("LIST ACDS SYSMOD(THTP450) .") to the #cmd("LIST") step.

The 4.5.0 form of the job has not been run on a system.

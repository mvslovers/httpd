# SMP-Installation httpd — was noch offen ist

**Das Paket wird generiert, nicht mehr von Hand gebaut.** `make package` erzeugt
aus dem `[distribution]`-Block in `project.toml` den SYSMOD, den Allokations-
und den Install-Job. Wie SMP4 dabei arbeitet und warum, steht in
`mbt/scripts/mbt/distribution.py` — dort sind die Handbuchstellen zitiert und
die Messungen vermerkt. Was ein Betreiber tun muss, steht in
[`docs/installation.md`](docs/installation.md).

Dieses Dokument hält nur noch das, was **für httpd offen** ist.

> *Stand: 2026-08-24, gegen `[distribution]` in `project.toml` abgeglichen.*

> **Die frühere Fassung verwies auf `../SMP-COOKBOOK.md` und
> `../SMP-INSTALLATION.md`. Beide Dateien gibt es nicht mehr.** Der Spike liegt
> noch in [`../smptest`](../smptest) — er hat 2026-08-08 gezeigt, dass SMP das
> LKLIB-Modul *kopiert* statt es neu zu binden, und genau darauf baut das
> generierte Paket auf.

---

## 1. Stand

**Vor dem 4.0.0-Tag ist nichts mehr offen, was MVS braucht.** O1 (FMID frei) und
O2 (Generalprobe) sind am 2026-08-24 gefahren, O3 (Webroot) ist mit Issue #252
umgesetzt. Was bleibt, ist O4 — eine Bequemlichkeitsfrage, keine Blockade — und
die Checksummen aus O3. Die erledigten Punkte bleiben mitsamt Belegen stehen:
sie sind das Protokoll des Release-Laufs, und die nächste Minor-Version fährt
dieselbe Strecke.

### ~~O1 — `THTP400` im CDS und ACDS prüfen~~ · **erledigt 2026-08-24**

Beide Stände melden die ID frei. Gefahren wurde auf jedem:

```
//LIST    EXEC SMPAPP
//SMPCNTL  DD  *
 LIST CDS  SYSMOD(THTP400) .
 LIST ACDS SYSMOD(THTP400) .
/*
```

| Stand | Job | CDS | ACDS |
|---|---|---|---|
| `mvsdev` (MVS/CE, HMASMP LVL 04.48) | `JOB01948` | RC 04, `NOT FOUND` | RC 04, `NOT FOUND` |
| `drnmig3a` (TK5, HMASMP LVL 04.48) | `JOB00028` | RC 04, `NOT FOUND` | RC 04, `NOT FOUND` |

Damit ist `THTP400` so belegt wie `TUFS120` und `TFTP100` vor ihren Tags —
der SMPPTS-Scan aus §3 zeigte nur *Empfangenes*, CDS und ACDS speichern
gehashte Membernamen, und nur der `LIST`-Job sieht, was wirklich angewendet
oder akzeptiert ist. Der Zonenoperand bleibt Pflicht (bares `LIST SYSMODS .`
ist SMP/E-Syntax → `HMA2033`), und qualifizieren auch: `LIST CDS .` gibt auf
MVS/CE 116 000 Zeilen aus.

**Zwei Dinge, die dabei gemessen wurden und beim nächsten Mal Zeit sparen:**

- Der `SMPAPP`-Procstep heißt auf beiden Ständen `HMASMP` und bringt **kein**
  `SMPCNTL` mit. Beide Formen funktionieren — `//HMASMP.SMPCNTL DD *` wie
  `//SMPCNTL DD *` unqualifiziert, weil die Prozedur nur einen Step hat.
- **Das Kriterium ist der Report, nicht der Returncode.** RC 04 steht am Ende
  jeder LIST-Ausführung; die Aussage steckt in `LIST SELECT SUMMARY REPORT`
  plus `THE FOLLOWING SELECTED ENTRIES WERE NOT FOUND` im **SMPOUT**. Wer nur
  den Job-RC abfragt, kann eine leere von einer treffenden Liste nicht
  unterscheiden.

### ~~O2 — Testinstallation mit Wegwerf-FMID `TTST400`~~ · **erledigt 2026-08-24**

Auf `mvsdev` gefahren, aus dem entpackten Archiv (`httpd-4.0.0-dev-dist.zip`),
also genau dem, was ein Betreiber bekommt. `fmid` stand dafür auf `TTST400` —
die Änderung ist **nicht** committet, `project.toml` sagt wieder `THTP400`.

| Schritt | Ergebnis |
|---|---|
| XMITs binär hochgeladen (`IBMUSER.HTTPD.LOAD.XMIT`, `…SAMP.XMIT`) | ok |
| `httpd-4.0.0-dev-alloc.jcl` (`JOB01972`) | CC 0000, `LINKLIB` + `AHTTPLOD` angelegt |
| `httpd-4.0.0-dev-inst.jcl` (`JOB01973`), nur die zwei `CHANGE.ME.*` ersetzt | **alle acht Schritte CC 0000**: DELOLD, RECV1, RECV2, RECV, APPLYCHK, APPLY, ACCEPT, CLEANUP |
| `LIST CDS/ACDS SYSMOD(TTST400)` (`JOB01974`) | RC 00, `JCLIN=YES`, REC/APP/ACC gestempelt, `MOD = HTTPD HTTPDM HTTPDMTT HTTPDSRV ABEND0C1` in **beiden** Zonen |
| `HTTPD.V4R0M0D.LINKLIB` / `.AHTTPLOD` | je fünf Module |
| `HTTPD.V4R0M0D.SAMPLIB` | `HTTPD`, `HTTPPRM0`, `HTTPWEBR` |
| `HTTPD.V4R0M0D.HTTPLOAD` | von CLEANUP verschrottet, wie vorgesehen |
| UCLIN + IDCAMS-Scratch (`JOB01975`) | CC 0000 |
| `LIST` danach (`JOB01976`, `JOB01977`) | `TTST400` in beiden Zonen weg, `THTP400` weiterhin frei |

Nach dem Lauf stehen auf `mvsdev` wieder nur `HTTPD.LINKLIB` und
`HTTPD.WWWROOT` — beides Site-Inhalt, unberührt.

**Was der Lauf über das Paket beweist**, über die Returncodes hinaus:

- Die `@VRM@`- und `@LINKLIB@`-Ersetzung kommt richtig auf dem Zielsystem an:
  der installierte `HTTPWEBR` sagt `DSN=HTTPD.V4R0M0D.WEBROOT.UFS`, die Proc
  `STEPLIB DD DSN=HTTPD.V4R0M0D.LINKLIB`.
- SMP kopiert, statt neu zu binden — die fünf Module stehen nach APPLY im
  Ziel und nach ACCEPT in der DLIB.
- Der UCLIN-Weg aus `docs/installation.md` §12 funktioniert wie beschrieben;
  er war bis dahin nur hergeleitet, nicht gefahren.
- Die Versionierung trägt: `V4R0M0D` (dev) kollidiert nicht mit dem späteren
  `V4R0M0`, und die Site-Platte `HTTPD.WWWROOT` liegt neben allem.

**Eine Falle, die dabei sichtbar wurde:** Datasets im Aufräumjob per IDCAMS
löschen, nicht über die REST-API — ein `DELETE /zosmf/restfiles/ds/…` lässt das
ENQ stehen, und der nächste `DISP=SHR`-Job hängt.

### O3 — UFS-Webroot: entschieden und umgesetzt (Issue #252)

Die Disk fährt **nicht** über SMP und **nicht** über XMIT, sondern als Datei im
Archiv und per **IND$FILE** aufs Ziel. Beides aus einem strukturellen Grund,
nicht aus Werkzeugmangel:

- SMP4 kennt für ein `DSORG=PS`/`RECFM=U`-Image keinen Elementtyp
  (`++MOD/MAC/SRC/MACUPD/SRCUPD/ZAP` ist die ganze Liste), und es ist
  **Site-Inhalt** — ein APPLY dürfte es nie anfassen.
- TSO RECEIVE legt sein Ziel selbst an und weigert sich, in ein bestehendes
  Dataset zu mischen: ein Transport nur für die Erstinstallation, für etwas, das
  der Betreiber ersetzt. (`xmit370` könnte das Format ohnehin nicht — Verzeichnis
  → PDS, `--recfm fb|f`, LRECL 80.)

Umgesetzt:

- **`make webroot`** baut `build/webroot/httpd-webroot.img` aus `static/` mit einem
  gepinnten `ufsd-utils` (1 MB = 256 Blöcke, `--owner`/`--group` gesetzt, damit
  im Artefakt keine Build-Maschinen-Userid steht). `package` und `dist` hängen
  davon ab, `[distribution] extra` legt es ins Archiv.
- **`samplib/httpwebr`** allokiert `HTTPD.@VRM@.WEBROOT.UFS`
  (`SPACE=(4096,256)`, `RECFM=U BLKSIZE=4096`, nur Primärextent).
- **`docs/installation.md` §9** ist der Betreiberweg: allokieren, IND$FILE,
  mounten, prüfen, ersetzen.

Was dabei gemessen wurde und in der Doku steht:

| Frage | Befund |
|---|---|
| Blockung | RECFM=U puffert über `__fputc` bis BLKSIZE (libc370 `@@fputc.c:24`) → `BLKSIZE(4096)` schreibt exakte 4096er-Blöcke; 1 MB = 256 ganze Blöcke |
| Optionen | Binär ist Default, RECFM dann `U` (ind_file370 `indparse.c:249`) |
| Vorher allokieren | IND$FILE nimmt ein existierendes Dataset DISP=SHR mitsamt DCB (`indmain.c:190`, `:273`) → Upload ohne jede Option, unabhängig vom IND$FILE-Build |
| Falle | dasselbe DISP=SHR schreibt auch in ein *gemountetes* Dataset → `UNMOUNT PATH=/www` ist ein nummerierter Schritt |
| Fehlerbild | BLKSIZE-Mismatch fällt beim MOUNT auf: `UFSD062E` (ufsd `ufsd#sbl.c:69`) |
| Codepage | keine Entscheidung nötig: `http_send_file()` übersetzt UFS-Dateien fest mit IBM-1047 (`src/httpfile.c:70`), unabhängig von `CODEPAGE=` — genau was `ufsd-utils cp` schreibt. Round-Trip byteweise gemessen, inkl. UTF-8; nur `0x85` und `0xF7` überleben ihn nicht |

Damit ist auch **U6** (Datenverlust beim Update) erledigt, ohne
`.SAMPLE`-Konstruktion: das Dataset trägt die Version im Namen, kann also die
Platte des Betreibers gar nicht treffen.

Offen geblieben:

- [ ] **U5-Rest** SHA256 der Release-Assets — mbt veröffentlicht heute keine
      Prüfsummen. Das Image trägt einen Erstellungszeitstempel, ist also nicht
      reproduzierbar; die Doku sagt das auch so, statt es zu versprechen.

### O4 — Setup-Job: ausliefern oder Handarbeit lassen?

`docs/installation.md` §7 lässt den Betreiber Proc und Konfigmember von Hand aus
der SAMPLIB kopieren. Das ist Absicht — *Produkt besitzt die Muster, Site
besitzt die Kopien* — aber ein **Muster** für den Kopierschritt wäre trotzdem
freundlicher als eine Prosaanweisung:

```jcl
//COPYPROC EXEC PGM=IEBCOPY
//IN       DD  DISP=SHR,DSN=HTTPD.@VRM@.SAMPLIB
//OUTP     DD  DISP=SHR,DSN=SYS2.PROCLIB
//OUTM     DD  DISP=SHR,DSN=SYS2.PARMLIB
//SYSIN    DD  *
  COPY OUTDD=OUTP,INDD=((IN,R))
  SELECT MEMBER=(HTTPD)
  COPY OUTDD=OUTM,INDD=((IN,R))
  SELECT MEMBER=(HTTPPRM0)
/*
```

Als weiteres SAMPLIB-Member (`HTTPSETU`) wäre das ein Zweizeiler in
`samplib/` — mit dem Haken, dass `@VRM@` dort ersetzt wird, `SYS2.PROCLIB` und
`SYS2.PARMLIB` aber geraten sind und auf TK5 falsch. Entscheidung offen.
Weder ufsd noch ftpd liefern so etwas aus.

---

## 2. Erledigt — was mbt heute übernimmt

Nicht neu planen. Die frühere §3 (Spike), §4 (SMP-Paket), §5 (SAMPLIB) sind
abgearbeitet oder ersetzt:

| Früher offen | Heute |
|---|---|
| Spike: kopiert SMP wirklich? | ✅ 2026-08-08 in `../smptest` bewiesen (`TSMP100`, Bitvergleich) |
| MCS-/JCLIN-/Allokationsjob-Generator | ✅ `mbt/scripts/mbt/distribution.py` + `mbtdist.py` |
| `[smp]`-Block in `project.toml` | ✅ als `[distribution]` / `[distribution.smp]`, mit **Datasetnamen** statt DDNAMEs — der DDNAME ist immer der letzte Qualifier |
| SAMPLIB als `++MAC` in den SYSMOD | ❌ verworfen: eine MCS-Karte endet in Spalte 72, `HTTPPRM0` hat Kommentarzeilen bis 80. Die SAMPLIB fährt als eigenes XMIT per TSO RECEIVE, außerhalb von SMP |
| Install-Job als SAMPLIB-Member | ❌ liegt im Archiv (`httpd-<version>-inst.jcl`), nicht in der SAMPLIB |

**Umgekehrt entschieden gegenüber der alten Fassung — zwei Punkte:**

- **Ziel ist `HTTPD.@VRM@.LINKLIB`, nicht `SYS2.LINKLIB`** (alte F1). Versionierte
  Produktdatasets heißen: kein IPL für einen APF-Eintrag, keine Kollision mit
  Systembibliotheken, jede Release-Stufe nebeneinander installierbar. Die Proc
  behält ihren STEPLIB und zieht ihn über `@LINKLIB@` aus dem Paket.
  Konsequenz für die alte Falle T6: der Selbstautorisierungsweg über SVC 244
  bleibt der Normalfall, `HTTPD002I AUTHORIZED BY SVC (MODULE KEY 8)` ist die
  erwartete Zeile. Der APF-Zweig ist trotzdem beschrieben — ein Betreiber, der
  die LINKLIB in `IEAAPF00` einträgt, bekommt `AUTHORIZED BY LIBRARY (MODULE
  KEY 0)`, und `docs/installation.md` §2 sagt, was das für die Modulspeicherung
  bedeutet.
- **ACCEPT fährt mit** (alte F4 sagte „nein, nie"). Der ACCEPT füllt die DLIB
  und ist damit die Basis, auf die ein späteres `RESTORE` eines **PTF**
  zurückgeht — ohne ihn würde RESTORE das Modul löschen statt es
  zurückzunehmen. Der Preis ist, dass die FMID selbst permanent wird: `RESTORE`
  scheitert, weil akzeptiert, und `REJECT`, weil der ACCEPT die MCS aus dem
  SMPPTS entfernt. Der Weg zurück ist `UCLIN`, beschrieben in
  `docs/installation.md` §12. **Service (PTFs) wird weiterhin nie akzeptiert.**

---

## 3. Verifiziertes Inventar

Nicht neu messen (🔬 gemessen · 📘 SMP4-Handbuch · ✅ Repo).

**Verfahren** 📘

- `++MOD(x) LKLIB(ddname)` liefert ein **fertig gebundenes** Loadmodul; ist das
  LMOD im JCLIN über einen IEBCOPY-Step definiert, kopiert SMP es statt zu binden.
- DDNAME = letzter Qualifier des Datasetnamens.
- Copy-Input im JCLIN muss **inline** hinter `//SYSIN DD *` stehen.
- `SELECT MEMBER=(…)` verwenden — sonst gilt die DLIB als *total* kopiert.
- SMP4 kennt keinen Datentyp: nur `++MOD/MAC/SRC/MACUPD/SRCUPD/ZAP`.

**Zielsystem `mvsdev`** 🔬

- `SMPAPP` und `SMPREC` liegen in `SYS2.PROCLIB`, nicht `SYS1.PROCLIB`.
- Beide Procs haben genau einen Step (`HMASMP`); DD-Overrides müssen ihn
  trotzdem als Qualifier tragen. Siehe Falle T9.
- Der Proc bringt `LINKLIB`, `LPALIB`, `PROCLIB`, `PARMLIB`, `SAMPLIB`,
  `ASAMPLIB`, `MACLIB`, `CMDLIB` mit — alle auf `SYS1.*`.

**SYSMOD-Inventar** 🔬 — `SYS1.SMPPTS` ist auslesbar, weil die MCS-Entries als
einzige *nicht* encodiert sind (dokumentierte Ausnahme 📘). 1544 SYSMODs im PTS,
darunter die Funktions-SYSMODs des Sysgens. **Frei im PTS:** `THTP400`,
`TTST400`. Das **CDS ist so nicht prüfbar** — Stichprobe der Membernamen:
`Ak\x7F\x1C]\x0D\xB6\x00`, `Jj7\x96\xFF\xDB\x3A\x00`. Deshalb O1.

> ⚠ Die alte Fassung nannte hier `TMVS100` als Vorschlag für mvsmf und riet
> davon ab. Der ökosystemweite Beschluss steht inzwischen im Root-`CLAUDE.md`:
> **nie `TMVS…`** (MVS/CE führt USERMODs `TMVS804/816/817`), mvsmf bekommt
> `TZMF010`.

**Artefaktseite** ✅ — **5** Loadmodule: `HTTPD` (AC=1), `HTTPDSRV`, `HTTPDM`,
`HTTPDMTT`, `ABEND0C1`. Die alte Fassung nannte sechs mit `HTTPJES2` und
`HTTPDSL`; beide sind seit 4.0.0 nicht mehr gebaut (mvsMFs jobs- und
dataset-API ersetzen sie, Quellen liegen in `tbd/`).

---

## 4. Fallen

Die, die noch gelten.

- **T1 — Die eiserne Regel für COPY-LMODs.** Ein Update liefert **immer das ganze
  Loadmodul** über LKLIB nach. Ein einzelnes Objekt-`++MOD` gegen ein COPY-LMOD
  bindet SMP allein und zerstört das Modul (📘 „no INCLUDE for the current
  version"). Nie ein Objektdeck in ein `++PTF` für httpd.

- **T2 — `norent` / `ac` bleiben bei ld370.** Der Grund, warum auf dem Host
  gebunden wird. Entsteht irgendwo ein JCLIN-Bindeschritt, wo keiner sein
  sollte, ist der Fehler hier.

- **T3 — ASCII→EBCDIC hängt an der Dateiendung.** `ufsd-utils cp` konvertiert
  nur bei bekannter Endung. Eine Datei ohne Endung oder mit `.tmpl` landet
  **verbatim als ASCII** in der Disk und ist auf MVS unlesbar — ohne
  Fehlermeldung. `-t` erzwingt Konvertierung, `-b` binär. Betrifft O3.

- **T4 — Das ausgelieferte `httpprm0` aktiviert nur MVSMF-Routen.** Für eine
  Testinstallation *ohne* mvsmf ist das die falsche Konfiguration.
  `docs/installation.md` §7 sagt das jetzt auch dem Betreiber; für den Test
  reicht eine eigene Minimalkonfiguration mit `DOCROOT=/www`.

- **T7 — DDNAME = letzter Qualifier.** `HTTPD.@VRM@.LINKLIB` hat den DDNAME
  `LINKLIB` und kollidiert damit zwangsläufig mit dem Proc-DD. Umbenennen hilft
  nicht, Override ist der Weg — mbt kennt die Proc-DDs (`SMPAPP_PROC_DDS`) und
  schreibt die Overrides selbst.

- **T9 — DD-Overrides brauchen den Proc-Step UND die richtige Reihenfolge.** Im
  Spike zweimal reingelaufen 🔬: `//LINKLIB DD …` nach `EXEC SMPAPP` ist **kein**
  Override, sondern eine zweite DD gleichen Namens — beide Datasets werden
  allokiert, SMP nimmt die des Procs, und jede Meldung sagt weiterhin
  `LIBRARY=LINKLIB`. Aus dem Log ist der Fehler nicht ablesbar. Richtig: erst
  alle Overrides, dann alle Ergänzungen, alle mit `HMASMP.`-Präfix. Der
  generierte Job macht das; der Kommentar im Job sagt auch, dass man ihn nicht
  umsortieren soll.

**Erledigt, nur zur Erinnerung warum:**

- ~~**T8 — `ACCEPT` ohne `COND`.**~~ Der generierte Job hängt jeden SMP-Step an
  den vorigen: `ACCEPT EXEC SMPAPP,COND=(0,NE,APPLY.HMASMP)`.
- ~~**T5 — mvsMF `S80A` nach wenigen Requests.**~~ Aus der Zeit vor dem
  Storage-Reclaim (#154/#174). Nicht als aktueller Befund weiterreichen; wenn
  ein Setup-Job Existenzprüfungen braucht, sind `IEFBR14`/`IDCAMS LISTCAT`
  trotzdem der robustere Weg.

---

## 5. Definition of Done — Testinstallation

- [ ] `RECEIVE` / `APPLY` mit RC ≤ 4, `APPLY CHECK` vorher sauber
- [ ] **Fünf** `HMA2380 COPY SUCCESSFUL`-Zeilen — eine je Modul. Weniger fällt
      nicht auf: der APPLY endet trotzdem RC 00
- [ ] Alle fünf Module liegen in `HTTPD.V4R0M0D.LINKLIB` und sind **bitgleich**
      mit `build/`
- [ ] `HTTPD.V4R0M0D.SAMPLIB` enthält `HTTPD` und `HTTPPRM0`, und der `STEPLIB`
      im Member zeigt auf `HTTPD.V4R0M0D.LINKLIB` (die `@LINKLIB@`-Ersetzung)
- [ ] `/S HTTPD` startet bis `HTTPD001I … READY`, `HTTPD002I` sagt, über welchen
      Weg autorisiert wurde
- [ ] `HTTPD.WEBROOT` angelegt, unter `/www` gemountet, `index.html` in EBCDIC
      lesbar — **nur wenn O3 bis dahin steht**; sonst gegen eine `MOD=`-Route
      prüfen und `HTTPD044W` als erwartet abhaken
- [ ] `curl http://mvsdev:8080/…` antwortet
- [ ] **Jede `MOD=`-Route der ausgelieferten `HTTPPRM0` einmal aufrufen**, nicht
      nur eine beliebige. Bei der 4.0.0-Probe sind das die drei `/zosmf/*`-Zeilen
      auf mvsMF — und `GET /zosmf/restjobs/jobs` ist der Aufruf, der #256
      gefunden hätte
- [ ] Aufräumen: UCLIN auf `TTST400`, Testdatasets gelöscht, `LIST CDS/ACDS
      SYSMOD(THTP400)` antwortet RC 04 mit leerer Liste

**Was diese Liste bis 4.0.0 nicht geprüft hat, und warum #256 durchkam.** Jeder
Punkt oben fragt, ob die *Installation* funktioniert: kopiert SMP die Module,
startet die STC, antwortet der Server. Keiner fragt, ob die installierte Prozedur
noch alloziert, was die Module öffnen, die sie lädt. Genau das war kaputt — die
Generalprobe lief vollständig CC 0000 durch, während `samplib/httpd` die
JES2-DDs nicht mehr hatte und mvsMFs Jobs-API damit tot war.

Der Grund ist eine Grenze, die nicht die des Repos ist: ein CGI wird per LINK-SVC
**in den Task der STC** geladen, hat also keine eigenen Allokationen und öffnet
jeden DD-Namen gegen die der Prozedur. Was die STC-JCL bereitstellt, gehört damit
zum Vertrag der Module, auch wenn keine Zeile Server-Quelltext es liest. Ein
`curl` auf *irgendeine* Route sieht das nicht; nur ein Aufruf auf die Route, die
das Modul tatsächlich benutzt.

Für die nächste Generalprobe heißt das: die Route-Liste der ausgelieferten
`HTTPPRM0` ist die Prüfliste, nicht ein einzelner Smoke-Test.

---

## 6. Nicht Teil davon

- Ausrollen auf ufsd, ftpd, mvsmf — ufsd und ftpd sind bereits umgestellt und
  sind die Referenz, an der httpds `[distribution]` sich orientiert.
- Objektdeck-Auslieferung — verworfen, siehe T1/T2.

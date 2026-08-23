# SMP-Installation httpd — was noch offen ist

**Das Paket wird generiert, nicht mehr von Hand gebaut.** `make package` erzeugt
aus dem `[distribution]`-Block in `project.toml` den SYSMOD, den Allokations-
und den Install-Job. Wie SMP4 dabei arbeitet und warum, steht in
`mbt/scripts/mbt/distribution.py` — dort sind die Handbuchstellen zitiert und
die Messungen vermerkt. Was ein Betreiber tun muss, steht in
[`docs/installation.md`](docs/installation.md).

Dieses Dokument hält nur noch das, was **für httpd offen** ist.

> *Stand: 2026-08-23, gegen `[distribution]` in `project.toml` abgeglichen.*

> **Die frühere Fassung verwies auf `../SMP-COOKBOOK.md` und
> `../SMP-INSTALLATION.md`. Beide Dateien gibt es nicht mehr.** Der Spike liegt
> noch in [`../smptest`](../smptest) — er hat 2026-08-08 gezeigt, dass SMP das
> LKLIB-Modul *kopiert* statt es neu zu binden, und genau darauf baut das
> generierte Paket auf.

---

## 1. Offen

### O1 — `THTP400` im CDS und ACDS prüfen · **vor dem 4.0.0-Tag**

Belegt ist bisher nur der **SMPPTS** auf `mvsdev`: 1544 SYSMODs gelesen, kein
`THTP400` (§3). Der PTS zeigt aber nur, was *empfangen* wurde. CDS und ACDS
speichern gehashte Membernamen — ein Namensvergleich dort liefert
Scheinergebnisse, wie in §3 gemessen. Gewissheit gibt nur ein `LIST`-Job, auf
**beiden** Ständen (`mvsdev` = MVS/CE, `drnmig3a` = TK5):

```
//LIST    EXEC SMPAPP
//SMPCNTL  DD  *
 LIST CDS  SYSMOD(THTP400) .
 LIST ACDS SYSMOD(THTP400) .
/*
```

**RC 04 mit leerer Liste heißt frei.** Der Zonenoperand ist Pflicht; bares
`LIST SYSMODS .` ist SMP/E-Syntax und endet in `HMA2033 SYNTAX ERROR`. Und
qualifizieren: `LIST CDS .` gibt auf MVS/CE 116 000 Zeilen aus.

Die FMID wird genau einmal vergeben. ufsd (`TUFS120`) und ftpd (`TFTP100`)
haben diesen Job vor ihrem Tag gefahren, httpd noch nicht — der Kommentar in
`project.toml` sagt das auch so.

### O2 — Testinstallation mit Wegwerf-FMID `TTST400`

Die Generalprobe aus einem `-dev`-Baum. Ein Testlauf unter `THTP400`, der
abbricht oder halb angewendet wird, belegt genau die ID, die das getaggte 4.0.0
braucht — und das CDS lässt sich hinterher nur mit `UCLIN` bereinigen
(`docs/installation.md` §11).

Mit mbt ist das eine Zeile: `fmid` in `project.toml` auf `TTST400` setzen,
`make package`, installieren — **und die Änderung nie committen**.

Die Datasetnamen kollidieren dabei nicht mit dem späteren Release, ohne dass
man etwas dafür tun muss: `@VRM@` kommt aus der Projektversion, und
`4.0.0-dev` ergibt `V4R0M0D`, das getaggte `4.0.0` dagegen `V4R0M0`. Der Test
lebt also in `HTTPD.V4R0M0D.*`, das Release in `HTTPD.V4R0M0.*`.

Aufräumen danach: UCLIN-Job aus `docs/installation.md` §11 mit `TTST400`, dann
die Datasets löschen, dann `LIST CDS/ACDS SYSMOD(THTP400)` — das ist zugleich
O1.

### O3 — UFS-Webroot: SMP kann das nicht ausliefern

Eine UFS-Disk ist ein `DSORG=PS`, `RECFM=U`, `BLKSIZE=4096`-Binärimage; SMP4
kennt dafür keinen Elementtyp (es kennt nur
`++MOD/MAC/SRC/MACUPD/SRCUPD/ZAP`). Sie ist außerdem **Site-Inhalt** — ein
erneutes APPLY dürfte sie nie anfassen.

Heute steht in `docs/installation.md`, dass ohne UFSD nur die `MOD=`-Routen
laufen und `HTTPD044W` kommt. Das ist ehrlich, aber es ist keine Auslieferung.
Offen:

- [ ] **U1** Image bauen — `ufsd-utils create webroot.img --size 10M`,
      `ufsd-utils cp -r static/ webroot.img:/`, danach `ufsd-utils ls -l`
      gegenprüfen (Falle T3)
- [ ] **U2** Hochladen: `ufsd-utils upload webroot.img --dsn HTTPD.WEBROOT`
- [ ] **U3** Mount dokumentieren — `UFSDPRMx` gehört **ufsd**, nicht httpd:
      `MOUNT DSN(HTTPD.WEBROOT) PATH(/www) MODE(RO)`, oder dynamisch
      `/F UFSD,MOUNT DSN=HTTPD.WEBROOT,PATH=/www,MODE=RO`. Kein `++MACUPD` in
      ufsds Parmlib
- [ ] **U4** Reihenfolge: **ufsd zuerst**, Dateisystem gemountet, `/www`
      existiert — *dann* httpd. Eine Laufzeitabhängigkeit, die SMP nicht
      abbilden kann
- [ ] **U5** `make webroot` + CI-Asset `httpd-<version>-webroot.img`, mit
      SHA256 statt eines Reproduzierbarkeitsversprechens
- [ ] **U6** **Update-Verhalten festlegen** — `.SAMPLE`-Name oder Existenzprüfung.
      Ein `ufsd-utils upload --replace` im Update-Job wäre Datenverlust beim
      Betreiber

`[distribution] extra = [...]` kann eine Datei ins Archiv legen, ohne SMP zu
behelligen — das ist der wahrscheinliche Weg für U5, sobald U6 entschieden ist.

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
  `docs/installation.md` §11. **Service (PTFs) wird weiterhin nie akzeptiert.**

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
- [ ] Aufräumen: UCLIN auf `TTST400`, Testdatasets gelöscht, `LIST CDS/ACDS
      SYSMOD(THTP400)` antwortet RC 04 mit leerer Liste

---

## 6. Nicht Teil davon

- Ausrollen auf ufsd, ftpd, mvsmf — ufsd und ftpd sind bereits umgestellt und
  sind die Referenz, an der httpds `[distribution]` sich orientiert.
- Objektdeck-Auslieferung — verworfen, siehe T1/T2.

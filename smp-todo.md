# SMP-Installation httpd — offene Punkte und TODOs

> **Erprobtes Rezept — hier zuerst nachsehen:** [`../SMP-COOKBOOK.md`](../SMP-COOKBOOK.md)
> **Konzept, Belege und Handbuchzitate:** [`../SMP-INSTALLATION.md`](../SMP-INSTALLATION.md)
>
> Dieses Dokument beantwortet nicht *wie* SMP funktioniert — das steht dort.
> Hier steht, **was für httpd noch fehlt** und in welcher Reihenfolge es
> abgearbeitet wird.

Ziel: eine **Testinstallation von httpd über SMP4** auf `mvsdev` — gebaut aus dem
aktuellen `4.0.0-dev`-Stand, inklusive SAMPLIB und UFS-Disk für das WWW-Root.
Sie ist die Generalprobe für das spätere Release der finalen `4.0.0`.

Ablauf: **§1 Fragerunde → §3 Spike → §4–§6 Ausbau → §8 Abnahme.**
Die Fragen in §1 sind vor dem ersten Schreibzugriff zu klären; §2 listet, was
bereits verifiziert ist und **nicht neu hergeleitet werden muss**.

> **Test ≠ Release.** `project.toml` steht auf `4.0.0-dev` — das ist der laufende
> Entwicklungsstand. Über SMP ausgeliefert wird später die finale, getaggte
> `4.0.0`. Die Testinstallation baut also aus einem Baum, der nicht das Release
> ist, und **muss deshalb ein Wegwerf-FMID benutzen** (`TTST400`).
>
> Warum das kein Detail ist: SMP führt sein Inventar im CDS. Ein Testlauf unter
> `THTP400`, der abbricht, halb angewendet wird oder aus einem `-dev`-Stand
> stammt, hinterlässt genau diese ID als belegt — und das echte 4.0.0 müsste
> dann gegen die eigenen Reste installiert werden. `THTP400` wird erst vom
> getaggten Release vergeben, und zwar genau einmal.

---

## 1. Fragerunde — vor dem ersten Schritt zu klären

| # | Frage | Vorschlag | blockiert |
|:--:|---|---|---|
| **F1** | **Wohin installiert SMP die Loadmodule?** Im Repo stehen drei Ziele nebeneinander: `make deploy` landet mangels `[deploy]`-Abschnitt auf `IBMUSER.HTTPD.V4R0M0.LINKLIB`, `samplib/httpd` hat `STEPLIB DD DSN=HTTPD.LINKLIB`, das Konzept schlägt `SYS2.LINKLIB` vor. | **`SYS2.LINKLIB`** — gemessen: APF-autorisiert *und* im LNKLST (§2). Die Proc bräuchte dann keinen STEPLIB mehr, und httpds Selbstautorisierung würde entfallen — **im Experiment beobachten**, s. T6 | JCLIN, DD-Override, Proc-Muster |
| **F2** | **FMID?** Muss 7 Zeichen haben und darf nicht kollidieren. `THTP400` ist im **SMPPTS frei** (1544 SYSMODs geprüft, §2) und passt zur lokalen Konvention. Restrisiko: das **CDS** ist nicht abfragbar, ein längst akzeptiertes SYSMOD könnte dort stehen, ohne im PTS zu sein. | `THTP400` übernehmen; Gewissheit nur über einen SMP-`LIST SYSMODS`-Job | alles |
| **F3** | **HLQ und Volume** für die Produkt-Datasets (DLIB, SAMPLIB, LKLIB)? | `SYS2.HTTPD.*`, Volume wie `MBT_MVS_DEPS_VOLUME` | Allokationsjob |
| ~~F4~~ | ~~ACCEPT mitfahren?~~ **Entschieden: nein, nie.** ACCEPT nimmt `RESTORE` weg und blockiert das erneute Empfangen desselben FMID mit geändertem Inhalt. Begründung und Konsequenzen: `../SMP-COOKBOOK.md` §5.1 | — | — |
| **F5** | **Schreibfreigabe für `mvsdev`** — und ist ein Backup von `SYS1.SMPCDS`/`SMPPTS` vorhanden? | Backup vor dem ersten RECEIVE; Wegwerf-FMID hinterher `REJECT`en | Spike |
| **F6** | **UFS: eigene Disk für `/www`** oder in ein bestehendes Dateisystem mounten? Welcher `UFSDPRMx`-Member? | eigene Disk `HTTPD.WEBROOT`, `MOUNT … PATH(/www) MODE(RO)` | Setup-Job |
| **F7** | **Port für den Test?** Default ist 8080. | 8080, sofern frei | Konfigmuster |
| **F8** | **SAMPLIB: eigenes Produkt-Dataset** (`SYS2.HTTPD.SAMPLIB`) oder in `SYS1.SAMPLIB`? | eigenes — sonst DD-Override-Konflikt mit dem Systemdataset | MCS, Setup-Job |
| **F9** | **Wegwerf-FMID für den Test?** `4.0.0-dev` ist der laufende Entwicklungsstand; über SMP ausgeliefert wird die finale `4.0.0`. Der Test läuft also gegen einen Baum, der **nicht** das Release ist. | **Ja** — Test auf `TTST400`, `THTP400` bleibt für das getaggte 4.0.0 reserviert. Begründung unten. | Spike, Aufräumen |

---

## 2. Was bereits feststeht

Nicht neu herleiten, nicht neu messen. Marker wie im Konzeptdokument
(✅ Repo · 🔬 gemessen · 📘 SMP4-Handbuch).

**Verfahren**

- `++MOD(x) LKLIB(ddname)` liefert ein **fertig gebundenes** Loadmodul; ist das
  LMOD im JCLIN über einen **IEBCOPY-Step** definiert, kopiert SMP es statt zu
  binden. 📘
- DDNAMEs = letzter Qualifier des Datasetnamens. 📘
- Copy-Input im JCLIN muss **inline** hinter `//SYSIN DD *` stehen. 📘
- `SELECT MEMBER=(…)` verwenden — sonst gilt die DLIB als *total* kopiert
  (Grenze: zwei Ziel-Bibliotheken). 📘
- SMP4 kennt **keinen** Datentyp: nur `++MOD/MAC/SRC/MACUPD/SRCUPD/ZAP`. 📘

**Zielsystem `mvsdev`**

- `SMPAPP` und `SMPREC` liegen in **`SYS2.PROCLIB`**, nicht `SYS1.PROCLIB`. 🔬
- Beide Procs haben genau einen Step (`HMASMP`) 🔬 — **DD-Overrides müssen
  trotzdem qualifiziert werden**: `//HMASMP.LINKLIB DD …`, nicht `//LINKLIB DD …`.
  Und die Override-Karten müssen **vor** allen hinzugefügten DDs stehen. Siehe
  Falle T9; im smptest-Spike gemessen. 🔬
- Der Proc bringt u. a. `LINKLIB`, `LPALIB`, `PROCLIB`, `PARMLIB`, `SAMPLIB`,
  `ASAMPLIB`, `MACLIB`, `CMDLIB` mit — alle auf `SYS1.*`. Wir überschreiben,
  was nach `SYS2.*` soll. 🔬
- `SYS2.LINKLIB` (20 Member) und `SYS2.PARMLIB` (9 Member) existieren. 🔬

**Artefaktseite**

- 6 Loadmodule gebaut: `HTTPD` (AC=1), `HTTPJES2`, `HTTPDM`, `HTTPDMTT`,
  `HTTPDSL`, `HTTPDSRV`. ✅
- `ld370 --pack … -xmit` → `upload_binary` → `RECEIVE` läuft produktiv in
  `mbt/scripts/mbtdeploy.py`. ✅
- `ufsd-utils` ist installiert (`~/go/bin/ufsd-utils`). 🔬

**Zielbibliothek `SYS2.LINKLIB`** 🔬

Aus `SYS1.PARMLIB` gelesen — beides spricht für `SYS2.LINKLIB` als Ziel:

```
IEAAPF00:  SYS2.LINKLIB MVS000,   USER BATCH LINKLIB      <- APF-autorisiert
LNKLST00:  SYS2.LINKLIB,          USER LOAD MODULE LIBRARY <- im Linklist
IEABLD00:  nur SYS1.LINKLIB-Module (HEWL, IFOXxx, LOGON …) <- kein BLDL-Eintrag
```

- **APF:** `SYS2.LINKLIB` ist autorisiert. httpd bringt sich heute selbst in den
  autorisierten Zustand (T6); von dort aus wäre es das von Anfang an. ✔
- **LNKLST:** die Module werden ohne STEPLIB gefunden. ✔
- **Kein IPL nötig**, damit ein *neues Member* sichtbar wird — in der residenten
  BLDL-Liste (`IEABLD00`) steht nichts aus `SYS2.LINKLIB`. Vorsicht dagegen beim
  **Erweitern oder Komprimieren** eines LNKLST-Datasets im laufenden Betrieb;
  ausreichend Platz vorher einplanen.

**SYSMOD-Inventar des Zielsystems** 🔬

`SYS1.SMPPTS` ist über die REST-API auslesbar — die Membernamen der MCS-Entries
sind **nicht** encodiert (dokumentierte Ausnahme 📘: *„With the exception of the
MCS entry in the SMPPTS dataset, these member names are encoded and cannot be
easily accessed by utilities other than SMP."*).

- **1544 SYSMODs** liegen im PTS. Darunter die Funktions-SYSMODs des Sysgens:
  `TIST801`, `TJES801`, `TMVS804`, `TMVS816`, `TMVS817`, `TNIP800`, `TTSO801` —
  also dieselbe Form `T` + 3 Buchstaben + 3 Ziffern, die wir vorschlagen.
- Ebenfalls da: `RAK0001` (RAKFs Usermod). **`TRKF200` dagegen nicht** — RAKF 2.0
  als Funktion ist auf diesem System nicht empfangen.
- **Frei im PTS:** `THTP400`, `TTST400`, `TUFS100`, `TFTP100`, `TLBC100`.
- ⚠ **`TMVS100` für mvsmf nicht verwenden** — `TMVS8xx` sind die
  MVS-Funktions-SYSMODs des Sysgens. Vorschlag stattdessen: `TZMF100`.

**Das CDS ist so *nicht* prüfbar.** Seine Membernamen sind echte Hashes
(Stichprobe: `Ak\x7F\x1C]\x0D\xB6\x00`, `Jj7\x96\xFF\xDB\x3A\x00`) — ein Namensvergleich dort
liefert nur Scheinergebnisse. Für Gewissheit braucht es einen SMP-`LIST`-Job.
Ebenso: `list_members()` scheitert an beiden Datasets, weil mvsMF die
Steuerzeichen als ungültiges JSON ausliefert; der Roh-Request unten umgeht das.

```python
# PTS-Inventar auslesen (read-only)
import re, base64, urllib.request
url  = "http://<host>:<port>/zosmf/restfiles/ds/SYS1.SMPPTS/member"
req  = urllib.request.Request(url)
req.add_header("Authorization", "Basic " + base64.b64encode(b"user:pass").decode())
req.add_header("Accept", "application/json")
txt  = urllib.request.urlopen(req, timeout=90).read().decode("utf-8", "replace")
ids  = sorted({n.strip() for n in re.findall(r'"member"\s*:\s*"(.*?)"', txt)
               if "\\u" not in n})          # MCS-Entries = lesbare SYSMOD-IDs
```

---

## 3. TODO — Spike (P0, zuerst)

> **Erledigt.** Der Spike wurde am 2026-08-08 im Projekt
> [`../smptest`](../smptest) gefahren, nicht mit `HTTPDM` — ein Modul, keine
> Abhängigkeiten, eigener FMID `TSMP100`. Ergebnis: SMP **kopiert** das
> LKLIB-Modul, `SYS2.CMDLIB(SMPTEST)` ist byteidentisch mit `build/SMPTEST`,
> ACCEPT trägt es in die DLIB, RESTORE nimmt es zurück. Belege und
> wiederverwendbare JCL in `../smptest/jcl/` und `../smptest/scripts/`.
>
> Für httpd bleibt davon: der Weg ist bestätigt, die Schritte unten sind die
> Vorlage für den ersten httpd-Lauf.

Die ursprüngliche Fragestellung: **kopiert SMP das LKLIB-Modul wirklich
unverändert?** Braucht **kein** neues mbt-Feature — alles von Hand, mit den
vorhandenen Upload-Wegen.

- [ ] **S1** `make modules` — Stand sicherstellen
- [ ] **S2** LKLIB auf MVS bringen — **Weg A (XMIT)**, der heute implementierte
      Pfad (§10.4 im Konzeptdokument):

      ```sh
      ld370 --pack build/HTTPDM.iebcopy -o build/httpd.deploy \
            -xmit --dsn HTTPD.V4R0M0.LKLIB
      # -> upload_binary (FB80) -> TSO RECEIVE -> HTTPD.V4R0M0.LKLIB
      ```

      `ld370 --pack` erhält dabei die Attribute **jedes** Members (AC, RENT,
      REUS) aus dem Bindelauf 📖 — deshalb überlebt `HTTPD`s `AC=1` den Transport.
      `--blocksize` muss beim Binden und Packen identisch sein (Default 15040).
      RECEIVE mergt nicht: Zielbibliothek vorher löschen, wie `mbtdeploy` es tut.
      *Alternative für später:* `-iebcopy` + Batch-`IEBCOPY` statt TSO — Vorteile
      und offene Fragen in §10.4.
- [ ] **S3** DLIB + SAMPLIB allokieren (IEFBR14): `SYS2.HTTPD.AHTTPLOD` als
      Loadlibrary (DCB wie `SYS2.LINKLIB`)
- [ ] **S4** MCS-Datei von Hand schreiben (~12 Zeilen) und als FB80 hochladen:

```
++FUNCTION(TTST400) .
++VER(Z038) /* httpd 4.0.0-dev - SPIKE, Wegwerf-FMID */ .
++JCLIN .
//TTST400  JOB 1,'HTTPD SPIKE',MSGLEVEL=1,CLASS=A
//COPYLOAD EXEC PGM=IEBCOPY
//AHTTPLOD DD  DISP=SHR,DSN=SYS2.HTTPD.AHTTPLOD
//LINKLIB  DD  DISP=SHR,DSN=SYS2.LINKLIB
//SYSIN    DD  *
  COPY INDD=AHTTPLOD,OUTDD=LINKLIB
  SELECT MEMBER=(HTTPDM)
/*
++MOD(HTTPDM) LKLIB(HTTPLOAD) DISTLIB(AHTTPLOD) .
```

- [ ] **S5** Install-Job von Hand: `RECEIVE SELECT(TTST400)` →
      `APPLY S(TTST400) CHECK` → `APPLY S(TTST400) DIS(WRITE)`, mit
      `//SMPPTFIN DD DSN=…`, `//HTTPLOAD DD …LKLIB`, `//AHTTPLOD DD …`,
      `//LINKLIB DD DISP=SHR,DSN=SYS2.LINKLIB`
- [ ] **S6** **Bitvergleich** `SYS2.LINKLIB(HTTPDM)` gegen `build/HTTPDM` —
      *das ist der eigentliche Test.* Gleich ⇒ SMP hat kopiert, nicht gebunden,
      und `norent`/`ac`/Entry stammen unverändert aus ld370.
- [ ] **S7** `ACCEPT S(TTST400) DIS(WRITE)` → landet das Modul in `AHTTPLOD`?
- [ ] **S8** `++PTF` mit demselben Modul, danach `RESTORE` — kommt der Vorstand
      sauber zurück?
- [ ] **S9** Aufräumen: `RESTORE` → `REJECT SELECT(TTST400)`, Test-Datasets
      löschen. **Kontrollieren, dass `THTP400` im CDS unberührt ist** — es wird
      erst vom getaggten 4.0.0 vergeben.

**Abbruchkriterium:** Wenn S6 zeigt, dass SMP doch bindet, stoppen und auf das
RAKF-Muster wechseln (Anhang C.7 im Konzeptdokument) — nicht auf Objektdecks.

---

## 4. TODO — SMP-Paket für httpd

Erst wenn §3 grün ist.

- [ ] **P1** FMID final vergeben (F2) und in `knowledge/` dokumentieren —
      `THTP400` **erst mit dem getaggten 4.0.0**, nicht aus dem `-dev`-Baum
- [ ] **P2** `[smp]`-Abschnitt in `project.toml`:

```toml
[smp]
fmid    = "THTP400"
system  = "Z038"
mode    = "lklib"
lklib   = "HTTPLOAD"            # DDNAME der hochgeladenen Loadlibrary
distlib = "AHTTPLOD"            # DDNAME der Loadmodul-DLIB
samplib = "AHTTPSAM"            # DDNAME der SAMPLIB-DLIB
target  = "SYS2.LINKLIB"        # aus F1
prereq  = ["TUFS100"]           # sonst aus [dependencies] abgeleitet
```

- [ ] **P3** MCS-Generator: `++FUNCTION`/`++VER`/`++JCLIN` + je `[[module]]` ein
      `++MOD … LKLIB` + je SAMPLIB-Member ein `++MAC`
- [ ] **P4** COPY-JCLIN-Generator: `SELECT MEMBER=(…)` aus den
      `[[module]]`-Namen
- [ ] **P5** Allokationsjob generieren (DLIB, SAMPLIB, ASAMPLIB, LKLIB)
- [ ] **P6** `make smppkg` → `dist/httpd-4.0.0.smpmcs` + `…lklib.xmit` +
      `…-inst.jcl` + `…-setup.jcl`
- [ ] **P7** `make smpinst` — Upload + Job absetzen + RC prüfen (**schreibt auf MVS**)
- [ ] **P8** Alle 6 Module statt nur `HTTPDM`; `HTTPD` trägt **AC=1** — prüfen,
      dass die Autorisierung den Kopiervorgang übersteht
- [ ] **P9** `++PTF`-Pfad (`make smpptf`) — **ganzes** LMOD via LKLIB
- [ ] **P10** **Modul entfällt / kommt hinzu:** Wenn sich die Modulliste ändert,
      braucht der SYSMOD ein neues `++JCLIN` mit angepasster
      `SELECT MEMBER=(…)`-Liste; ein entfallenes Modul zusätzlich per
      `++MOD(x) DELETE`. Generator muss das aus dem Diff der `[[module]]`-Namen
      gegen den letzten Release ableiten.
- [ ] **P11** **Aliase:** httpd hat heute keine. Falls je welche entstehen,
      müssen sie mit `TALIAS(…)` deklariert werden **und** in der LKLIB
      vorhanden sein — sonst kopiert SMP sie nicht 📘.

---

## 5. TODO — SAMPLIB

Siehe §14.5 im Konzeptdokument. Kernregel:

> **Produkt besitzt die Muster, Site besitzt die Kopien.**
> `SYS2.HTTPD.SAMPLIB(HTTPD)` gehört SMP — `SYS2.PROCLIB(HTTPD)` nicht.

Grund: `samplib/httpd` enthält `STEPLIB`, `VOL=SER=MVS000`, REGION, Port — alles
Werte, die der Betreiber ändert. Besäße SMP die aktive Proc, wäre jede Anpassung
beim nächsten APPLY weg.

- [ ] **L1** `SYS2.HTTPD.SAMPLIB` + `.ASAMPLIB` allokieren (FB80)
- [ ] **L2** Bestehende Muster als `++MAC` aufnehmen:
      `samplib/httpd` → Member `HTTPD`, `samplib/httpprm0` → Member `HTTPPRM0`
- [ ] **L3** **Generierte** Member ergänzen — sie tragen FMID, Version und
      Datasetnamen und müssen zum Release passen:
      - `HTTPINST` — der SMP-Install-Job
      - `HTTPSETU` — der Setup-Job (Disk, Mount, Kopien)
      - `HTTPWEB` — JCL-Muster für den Disk-Upload
- [ ] **L4** DD-Override im APPLY/ACCEPT-Step:
      `//SAMPLIB DD DISP=SHR,DSN=SYS2.HTTPD.SAMPLIB` (letzter Qualifier passt)
- [ ] **L5** Kopierschritt **im Setup-Job**, nicht im Install-Job:

```jcl
//COPYPROC EXEC PGM=IEBCOPY
//IN       DD  DISP=SHR,DSN=SYS2.HTTPD.SAMPLIB
//OUTP     DD  DISP=SHR,DSN=SYS2.PROCLIB
//OUTM     DD  DISP=SHR,DSN=SYS2.PARMLIB
//SYSIN    DD  *
  COPY OUTDD=OUTP,INDD=((IN,R))
  SELECT MEMBER=(HTTPD)
  COPY OUTDD=OUTM,INDD=((IN,R))
  SELECT MEMBER=(HTTPPRM0)
/*
```

- [ ] **L6** Für die Testinstallation eine **eigene Minimalkonfiguration**
      erzeugen — siehe Falle T4

---

## 6. TODO — UFS-Disk / WWW-Root

Siehe §14.1–§14.4 im Konzeptdokument. SMP4 kann das **nicht** ausliefern: eine
UFS-Disk ist ein `DSORG=PS`, `RECFM=U`, `BLKSIZE=4096`-Binärimage, und es gibt
keinen passenden Elementtyp. Sie ist außerdem **Site-Inhalt** — ein erneutes
APPLY dürfte sie nie anfassen.

- [ ] **U1** Image bauen:

```sh
ufsd-utils create webroot.img --size 10M
ufsd-utils cp -r static/ webroot.img:/
ufsd-utils ls -l webroot.img:/        # Kontrolle, s. Falle T3
```

- [ ] **U2** Hochladen: `ufsd-utils upload webroot.img --dsn HTTPD.WEBROOT`
      (legt das Dataset mit an)
- [ ] **U3** Mount eintragen — **`UFSDPRMx` gehört ufsd, nicht httpd** (F6):
      `MOUNT DSN(HTTPD.WEBROOT) PATH(/www) MODE(RO)`
      oder dynamisch `/F UFSD,MOUNT DSN=HTTPD.WEBROOT,PATH=/www,MODE=RO`.
      **Kein `++MACUPD` in ufsds Parmlib** — Muster ausliefern, Schritt
      dokumentieren.
- [ ] **U4** Laufzeitreihenfolge sicherstellen: **ufsd zuerst starten**,
      Dateisystem gemountet, `/www` existiert — *dann* httpd. `DOCROOT` ist eine
      Laufzeitabhängigkeit, die SMP nicht abbilden kann.
- [ ] **U5** `make webroot` + CI-Asset `httpd-4.0.0-webroot.img` (§20 im Konzeptdokument), mit SHA256-Summe statt Reproduzierbarkeitsversprechen
- [ ] **U6** **Update-Verhalten festlegen** (offen): `.SAMPLE`-Name oder
      Existenzprüfung im Setup-Job. `ufsd-utils upload --replace` in einem
      Update-Job wäre ein Datenverlust beim Betreiber.

---

## 7. Fallen

Jede einzelne kostet sonst einen Arbeitstag.

- **T1 — Die eiserne Regel für COPY-LMODs.** Ein Update liefert **immer das
  ganze Loadmodul** über LKLIB nach. Ein einzelnes Objekt-`++MOD` gegen ein
  COPY-LMOD bindet SMP *allein* und zerstört das Modul (📘 „no INCLUDE for the
  current version"). Nie ein Objektdeck in ein `++PTF` für httpd.

- **T2 — `norent` / `ac` bleiben bei ld370.** Der Grund, warum wir auf dem Host
  binden: ld370s Template markiert alles RENT+REUS, C-Module mit mutable statics
  nehmen dann bei TSO-Load einen S0C4 (dokumentiert in RAKFs
  `APPLICATIONS/project.toml`). Wenn ein JCLIN-Bindeschritt entsteht, wo keiner
  sein sollte, ist der Fehler *hier*.

- **T3 — ASCII→EBCDIC hängt an der Dateiendung.** `ufsd-utils cp` konvertiert
  nur bei bekannter Endung (`.html`, `.css`, `.js`, …). Eine Datei ohne Endung
  oder mit `.tmpl` landet **verbatim als ASCII** in der Disk und ist auf MVS
  unlesbar — ohne Fehlermeldung. `-t` erzwingt Konvertierung, `-b` erzwingt
  binär. Ausgabe von `ufsd-utils ls -l` gegenprüfen.

- **T4 — Das ausgelieferte `httpprm0` aktiviert nur MVSMF-Routen.** Die einzigen
  unkommentierten Zeilen im Muster sind `MOD=MVSMF /zosmf/…`. Für eine
  httpd-Testinstallation **ohne** mvsmf ist das die falsche Konfiguration —
  eigene Minimalkonfiguration mit `DOCROOT=/www` und ggf. `MOD=HTTPDSRV /.dsrv`
  verwenden.

- **T5 — mvsMF wirft `S80A ABEND` nach wenigen Requests.** Ein HTTP 503 heißt
  dann **nicht** „Dataset fehlt". Außerdem liefern `dataset_exists()` und
  `list_datasets()` gegen diesen Server durchgehend `False`/`[]` — auch für
  nachweislich existierende Datasets. Verlässlich ist nur `list_members()`.
  Existenzprüfungen im Setup-Job besser per JCL (`IEFBR14`/`IDCAMS LISTCAT`).

- **T9 — DD-Overrides brauchen den Proc-Step UND die richtige Reihenfolge.**
  Im smptest-Spike zweimal reingelaufen 🔬. `//CMDLIB DD DSN=SYS2.CMDLIB` nach
  `EXEC SMPAPP` ist **kein** Override, sondern eine zweite DD gleichen Namens:
  beide Datasets werden allokiert, SMP nimmt die des Procs, das Modul landet in
  `SYS1.CMDLIB` — und jede SMP-Meldung sagt weiterhin `LIBRARY=CMDLIB`, der
  Fehler ist also aus dem Log **nicht** ablesbar. Qualifizieren allein reicht
  auch nicht: stehen hinzugefügte DDs (`SMPTLOAD`, `ASMPTEST`) vor der
  Override-Karte, wird sie ebenfalls ignoriert. Richtig ist:
  **erst alle Overrides, dann alle Ergänzungen**, alle mit `HMASMP.`-Präfix.
  Kontrolle: nachsehen, wo das Member wirklich liegt — nicht dem RC glauben.

- **T6 — Die Autorisierung ändert ihren Weg, und das will beobachtet werden.**
  httpd hängt heute *nicht* davon ab, dass die STEPLIB autorisiert ist — es
  **autorisiert sich selbst**. `httpd.c` verzweigt beim Start ✅:

  ```c
  if (crt->crtopts & CRTOPTS_AUTH)  rc = auth_setup(argv[0]);   /* schon autorisiert */
  else                              rc = unauth_setup(argv[0]); /* holt sich die Autorisierung */
  ```

  `unauth_setup()` ruft `__autask()` (SVC 244 → JSCBAUTH) und danach
  `__austep()`, um die STEPLIB nachträglich APF-autorisiert zu machen —
  Meldungen `HTTPD011I … via SVC 244` und `HTTPD013I STEPLIB is now APF
  authorized`. Deshalb funktioniert die heutige Proc mit
  `STEPLIB DD DSN=HTTPD.LINKLIB` auch ohne APF-Eintrag.

  Nach einer Installation in die autorisierte LNKLST-Verkettung sollte der
  `auth_setup()`-Zweig greifen (`HTTPD010I … is APF authorized`) und die ganze
  Selbstautorisierung entfallen. **Das ist während der Installationsexperimente
  empirisch zu prüfen**, nicht vorher zu behaupten:

  - Welcher Zweig wird tatsächlich genommen? (Meldung im Log: `HTTPD010I` vs.
    `HTTPD011I`/`HTTPD013I`)
  - Ist `__austep()` dann überflüssig, harmlos oder störend?
  - Braucht es eine zusätzliche Prüfung im Code — oder reicht `CRTOPTS_AUTH`
    bereits? Der Verdacht ist, dass es schon reicht.

  Nebenbefund: SVC 244 kommt von RAKFs Usermod `RAK0001` — der liegt auf
  `mvsdev` im PTS 🔬. Der heutige Selbstautorisierungsweg hängt also an RAKF.
  **Kommt nach dem Spike, blockiert ihn nicht.**

- **T7 — DDNAME = letzter Qualifier.** `SYS2.HTTPD.LINKLIB` hätte ebenfalls den
  DDNAME `LINKLIB` — die Kollision mit dem Proc-DD verschwindet nicht durch
  Umbenennen. Override ist der Weg.

- **T8 — `ACCEPT` ohne `COND`.** RAKF macht das und akzeptiert damit auch nach
  einem gescheiterten APPLY. Getrennte Jobs (F4).

---

## 8. Definition of Done — Testinstallation

- [ ] `RECEIVE` / `APPLY` laufen mit RC ≤ 4, `APPLY CHECK` vorher sauber
- [ ] Alle 6 Module liegen in der Zielbibliothek und sind **bitgleich** mit
      `build/`
- [ ] `SYS2.HTTPD.SAMPLIB` enthält `HTTPD`, `HTTPPRM0`, `HTTPINST`, `HTTPSETU`,
      `HTTPWEB`
- [ ] Setup-Job hat Proc und Konfig nach `SYS2.PROCLIB`/`SYS2.PARMLIB` kopiert
- [ ] `HTTPD.WEBROOT` ist angelegt, unter `/www` gemountet, `index.html` darin
      lesbar (EBCDIC!)
- [ ] `/S HTTPD` startet, `curl http://mvsdev:8080/` liefert `index.html`
- [ ] `RESTORE` bringt einen sauberen Vorstand zurück
- [ ] Wegwerf-FMID `TTST400` ist `REJECT`ed, Testdatasets sind gelöscht
- [ ] `THTP400` ist im CDS **nicht** belegt — reserviert für das getaggte 4.0.0

---

## 9. Nicht Teil der Testinstallation

- Ausrollen auf ufsd, ftpd, mvsmf (dieselbe Mechanik, §21 Roadmap im Konzeptdokument)
- CI-Integration der Artefaktmatrix (§20 im Konzeptdokument)
- Objektdeck-Auslieferung — **verworfen**, Begründung in Anhang C

# Repro: Hercules-Absturz durch blockierenden X'75'-SEND (Stalled Client)

> **Status 2026-08-18: auf `mvsdev` GEPATCHT und GRÜN.** `tcpip.c` SEND ist
> non-blocking (Abschnitt 5), der RED-Lauf lässt sich dort nicht mehr
> reproduzieren. Das Dokument bleibt gültig für ungepatchte Emulatoren, für
> Regressionstests nach einem Hercules-Update (ein Rebuild via hercules-helper
> verwirft den lokalen Patch!) und als Vorlage für den Upstream-Report.

Reproduktion und Rot/Grün-Test für den Hercules-Absturz, der beim Live-Test von
PR #206 aufgedeckt wurde. Das Dokument beschreibt **einen Hercules-Plattformfehler**,
nicht einen httpd-Fehler — httpd ist nur das Transportmittel, um ihn auszulösen.

**Kurzfassung:** Ein Client, der eine HTTP-Verbindung offen hält und *aufhört zu
lesen*, füllt den Host-Sendepuffer des Server-Sockets. Das nächste `send()` des
MVS-Gasts blockiert dann — und zwar direkt auf dem CPU-Thread des Emulators, weil
die X'75'-Socket-Emulation (`tcpip.c`) den Host-Socket blockierend bedient. Ein
eingefrorener CP macht keinen Instruktionsfortschritt; der Hercules-Watchdog
wertet das als Deadlock und führt **absichtlich `CRASH()`** aus. Der ganze
Emulator stirbt (SIGSEGV), nicht nur die STC.

Latenzeinordnung: Das ist **kein Testartefakt**, sondern ein latentes
Produktionsrisiko (Client schläft ein / VPN-Abriss ohne RST mitten in einer
größeren Antwort). Wir sind nur deshalb erst beim #206-Test hineingelaufen, weil
dessen Protokoll als erstes die auslösende Verkehrsform erzeugt hat (offene
Verbindung + gar nicht lesen + Multi-MB-Rückstau über HTTP/1.1-Keep-Alive).

---

## 1. Root Cause (nachgewiesen per gdb, 2026-08-18)

| Schicht | Datei | Verhalten |
|---|---|---|
| httpd | `src/httpsend.c` | setzt Socket auf `FIONBIO`, verlässt sich auf `EWOULDBLOCK` |
| libc370 `send()` | `src/dyn75/@@75send.c` | roher X'75'-Wrapper, Funktionscode 10 — keine Non-Blocking-Emulation |
| Hercules IOCTL | `tcpip.c` (`case 15`) | Gast-`FIONBIO` setzt **nur** das Software-Flag `Ccom_blk` |
| Hercules RECV / ACCEPT | `tcpip.c` (`case 11` / `case 8`) | prüfen `Ccom_blk`, pollen per `select` mit Zero-Timeout → blockieren nie |
| Hercules **SEND** | `tcpip.c` (`case 10`, ~Zeile 514) | **ignoriert `Ccom_blk`, ruft blockierendes Host-`send()`** — kein Timeout, kein `MSG_DONTWAIT` |
| Hercules Watchdog | `impl.c` (Erkennung ~Zeile 634, `CRASH()` bei ~Zeile 731) | misst Instruktionsfortschritt; ein hängender CP → `CRASH()` |

Beweis-Backtrace (`~/MVSCE/herc-crash-bt.txt`), beide CPU-Threads standen im
Host-`send()`:

```
#3  send () from /lib/x86_64-linux-gnu/libc.so.6
#4  EZASOKET (func=..., aux1=4, ...) at ../tcpip.c:514
#5  lar_tcpip (regs=...) at ../tcpip.c:1020
#6  s370_tcpip (...) at ../x75.c:124
#7  s370_run_cpu (...) at ../cpu.c:2102
#8  cpu_thread (...) at ../cpu.c:2398
```

Der Watchdog-Thread beim Suizid:

```
Thread 18 "watchdog_thread" received signal SIGSEGV
#0  watchdog_thread (...) at ../impl.c:731     <-- CRASH()  ("crash dump for offline analysis")
```

**Wichtig:** Der Watchdog feuert schon bei **einem** hängenden CP. Die Debugger-
Ausnahme, die den Watchdog neutralisieren würde, ist im Quelltext
`#if defined(_MSVC_)` — also **nur Windows**. Unter Linux gibt es keinen
Runtime-Schalter, nur die Compile-Option `OPTION_WATCHDOG`.

---

## 2. Vorbedingungen (alle drei müssen zusammenkommen)

1. **Client hält die Verbindung offen und liest gar nichts** — kein Abbruch
   (RST → `send()` liefert sofort Fehler, kein Block), kein langsames Lesen
   (`--limit-rate` → stetiger Fortschritt, kein Block). Genau „offen + still".
2. **Rückstau > Host-Sendepuffer.** Der Puffer startet bei ~16 KB und tunt bis
   `tcp_wmem`-Max = 4 MB hoch. Um ihn sicher zu überfüllen, mehrere MB auf
   **einer** Verbindung aufstauen → HTTP/1.1-Keep-Alive-Pipelining vieler
   Antworten. (Vor 4.0.0 gab es keine persistenten Verbindungen, daher war der
   Rückstau nie groß genug — mit ein Grund, warum der Fehler nie auftrat.)
3. **Haltezeit > Watchdog-Intervall.** Kurze Halten (20–32 s) überlebten in den
   Versuchen; 40 s+ lösten den `CRASH()` aus.

Warum unsere bisherigen Tests **nie** crashten:
- #199-Salven (`--max-time 0.3`) brechen ab → RST → `send()` liefert Fehler, kein Block.
- Slow-Reader (`--limit-rate`) lesen stetig → Puffer drainiert → jedes `send()` kommt durch.
- Der #199-Spin selbst führt Milliarden Instruktionen aus (54–137 MIPS) → für den
  Watchdog kerngesund, deshalb Wedges/Latenz-Rampen, aber nie ein Emulator-Absturz.

---

## 3. Umgebung / Voraussetzungen

- **Host:** `mvsdev.lan` (Linux 6.12.88 Debian 13, 4 Cores).
- **Hercules:** `/usr/local/hercules/bin/hercules`, 4.10.0.11725-SDL-DEV-g9f8803a1.
  Quellbaum: `~/hercules/hyperion`. Start: `~/MVSCE/start_mvs.sh` (tmux-Session `0`).
  Log: `~/MVSCE/hercules.log`.
- **Konsole:** Hercules-Webkonsole `http://mvsdev.lan:8181/cgi-bin/tasks/syslog`
  (POST-Feld `command`, z. B. `/P HTTPD`) — oder direkt in der tmux-Session.
- **httpd:** 4.0.0-dev auf `mvsdev.lan:8080`, mit registriertem `/.dm` (Modul
  HTTPDM). `/.dm` braucht Basic Auth (IBMUSER). Parmlib-Zeile: `CGI HTTPDM /.dm`.
- Der Client wird **von mvsdev selbst** ausgeführt. Grund: Linux respektiert ein
  kleines `SO_RCVBUF` exakt; macOS tunt es auf ~4 MB hoch und verschleiert damit
  die Schwelle. Loopback (127.0.0.1) hält außerdem die BDP klein.

### Core-Dumps aktivieren (einmalig, für die gdb-Auswertung)

Der Prozess ist standardmäßig **non-dumpable** (File-Capability `cap_sys_nice=eip`
auf der Binary) — deshalb entsteht trotz „Creating crash dump…" nie ein Core, und
ein Same-User-`gdb`-Attach scheitert mit `ptrace: Operation not permitted`.

```sh
# einmal, als Admin auf mvsdev:
sudo /usr/sbin/setcap -r /usr/local/hercules/bin/hercules   # Capability entfernen
# ulimit -c unlimited steht bereits in start_mvs.sh
# core_pattern ist "core" -> Core landet in ~/MVSCE/core
sudo apt install -y gdb                                     # gdb fehlt sonst auf mvsdev
```

Nach `setcap -r` genügt für alles Weitere der normale User (kein sudo mehr nötig).
Kosten: Hercules kann seine Thread-Prioritäten nicht mehr anheben (harmlos auf Dev).

---

## 4. Reproduktion (RED — ungepatchter Emulator)

### Schritt 1 — Vorprüfung

```sh
ssh mvsdev.lan 'pgrep -f "hercules -f" >/dev/null && echo hercules-up || echo hercules-DOWN'
curl -s -o /dev/null -w "httpd: %{http_code}\n" -u IBMUSER:<pass> \
     "http://mvsdev.lan:8080/.dsrv?target=HTTPD"     # erwartet 200
```

### Schritt 2 — Stall-Client auf mvsdev ablegen

`~/stalltest.py` (auf mvsdev):

```python
import base64, socket, sys, time
host, port = '127.0.0.1', 8080
user, pw, hold, n, cap = sys.argv[1], sys.argv[2], float(sys.argv[3]), int(sys.argv[4]), float(sys.argv[5])
auth = base64.b64encode(('%s:%s' % (user, pw)).encode()).decode()
one = ('GET /.dm?m=0&l=4096&c=16 HTTP/1.1\r\nHost: %s:%d\r\n'
       'Authorization: Basic %s\r\n\r\n' % (host, port, auth))
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 2048)   # Linux haelt sich daran
s.settimeout(15); s.connect((host, port))
s.sendall((one * n).encode())                             # n Requests pipelinen
t0 = time.time()
print('%d requests (~%.1f MB) pipelined, lese NICHTS fuer %.0fs' % (n, n*94/1024.0, hold))
sys.stdout.flush()
time.sleep(hold)                                          # Fenster zu -> Server blockiert
s.settimeout(5); total = 0; closed = False; t1 = time.time()
try:
    while time.time() - t1 < cap:
        b = s.recv(65536)
        if not b: closed = True; break
        total += len(b)
except socket.timeout: pass
except (ConnectionResetError, OSError): closed = True
print('drained %.2f MB in %.1fs, server_closed=%s' % (total/1048576.0, time.time()-t1, closed))
```

### Schritt 3 — auslösen

```sh
ssh mvsdev.lan 'python3 ~/stalltest.py IBMUSER <pass> 40 150 45'
```

`40` = 40 s nicht lesen (> Watchdog-Intervall), `150` = 150 pipelined Requests
(~14 MB ≫ 4 MB Puffer), `45` = danach bis zu 45 s draining messen.

### RED — erwartetes Ergebnis (ungepatcht)

Innerhalb von ~30–40 s stirbt der Emulator. Belege:

- `pgrep hercules` liefert nichts mehr; Ports 8080/8181/2121 sind zu.
- tmux-Session `0` bzw. `~/MVSCE/hercules.log` endet mit:
  ```
  HHC00822S PROCESSOR CPnn APPEARS TO BE HUNG!
  HHC00007I Previous message from function 'watchdog_thread' at impl.c(634)
  HHC02324I CPnn: PSW=...  INST=75005000  TCPIP 0,0(0,5)      <-- rauchende Pistole: X'75' SEND
  HHC90026W No threads found with tid ffffffffffffffff.
      +++ OOPS! +++
  Hercules has crashed! (Segmentation fault)
  ```
- Der Stall setzt bereits **vor** einem `P HTTPD` ein: `F HTTPD,D THREADS` bekommt
  ab dem Freeze keine Antwort mehr (die ganze Maschine ist stumm, nicht nur httpd).

---

## 5. Rot/Grün-Test nach dem `tcpip.c`-Patch (GREEN)

Patch-Idee (in `~/hercules/hyperion/tcpip.c`, `case 10: /* SEND */`): SEND vor dem
`send()` non-blocking machen — analog zum RECV daneben per `select`-mit-Timeout
auf `writefds`, oder `MSG_DONTWAIT` — und bei „würde blockieren" `hEWOULDBLOCK`
an den Gast zurückgeben (Flag `Ccom_blk` respektieren). Danach neu bauen und
Hercules neu starten.

**Angewandter Patch (mvsdev, 2026-08-18)** — Backup: `tcpip.c.orig-preSENDfix`:

```c
    case 10: /* SEND */
        if (check_not_sock (aux1, t)) return;
        /* X'75' laeuft auf dem CPU-Thread -> darf nie blockieren */
#if defined( MSG_DONTWAIT )
        l = send (Ccom_han [aux1], t->buffer_in, t->len_in, MSG_DONTWAIT);
#else
        timeout.tv_sec = 0; timeout.tv_usec = 0;
        FD_ZERO (&sockets); FD_SET (Ccom_han [aux1], &sockets);
        if (select (Ccom_han [aux1] + 1, NULL, &sockets, NULL, &timeout) == 0) {
            if (Ccom_blk [aux1]) { t->ret_cd = -2; }
            else { Cerr [aux1] = hEWOULDBLOCK; t->ret_cd = -1; }
            return;
        }
        l = send (Ccom_han [aux1], t->buffer_in, t->len_in, 0);
#endif
        if (l == SOCKET_ERROR) {
            Cerr [aux1] = Get_errno ();
            if (Cerr [aux1] == hEWOULDBLOCK && Ccom_blk [aux1]) t->ret_cd = -2;
            else                                               t->ret_cd = -1;
            return;
        }
        t->ret_cd = l;
        return;
```

Bauen (ohne sudo) und installieren (mit sudo):

```sh
ssh mvsdev.lan 'cd ~/hercules/hyperion/build && make'      # ~10 min
ssh -t mvsdev.lan 'cd ~/hercules/hyperion/build && sudo make install'
# danach Hercules in der tmux-Session neu starten (./start_mvs.sh)
# Version meldet sich dann als ...-g9f8803a1-modified
```

Denselben Befehl aus Schritt 3 erneut ausführen. **GREEN** heißt:

- **Emulator überlebt:** `pgrep hercules` liefert weiterhin die PID; kein
  `HHC00822S`, kein OOPS im Log.
- **Server bricht den Stall sauber ab:** Das Gast-`send()` bekommt jetzt
  `EWOULDBLOCK`; httpds Stall-Budget (#200/#202, `SEND_STALL_MAX`×`SEND_STALL_PAUSE`
  = 10 s) greift, danach `CSTATE_DONE` und Verbindungsabbruch. Der Client meldet
  etwa: `drained ~X MB ..., server_closed=True` nach rund 10 s Stillstand statt
  Emulator-Tod.
- **httpd bleibt bedienbar:** Parallel `F HTTPD,D THREADS` antwortet weiter; eine
  zweite Verbindung wird normal bedient; kein Worker bleibt wedged.

### Gemessenes GREEN-Ergebnis (mvsdev, 2026-08-18, gepatchte Binary)

```
150 requests (~13.8 MB) pipelined, lese NICHTS fuer 40s
drained 0.00 MB in 0.0s, server_closed=True
```

Parallele Lebendprüfung im Sekundentakt während des kompletten Laufs:

```
t≈5s .. t≈40s: httpd=200 in 0.08-0.09s, hercules-prozesse=1   (durchgehend)
```

Also: Emulator überlebt, kein `HHC00822S`, kein OOPS. httpd lieferte während des
gesamten Stalls unbeeindruckt in ~90 ms aus. Der Stall-Client bekam **0 Bytes**
und einen serverseitigen Close — das Stall-Budget hat gefeuert, bevor auch nur
ein Byte den Weg nach draußen fand (der Host-Puffer wird nicht mehr blind
vollgeschrieben). Nachkontrollen: voller Dump 30 240 B in 0,17 s korrekt,
Abbruch-Salve (40 Verbindungen) danach 200 in 0,07 s, Worker-Pool 8 WAITING +
1 RUNNING (der Prüfling selbst) — kein wedged Worker.

**Damit ist das httpd-Stall-Budget aus #200/#202 zum ersten Mal überhaupt live
scharf gewesen** — vorher war es auf dieser Plattform unerreichbar.

### #206-Nachweis — GRÜN (mvsdev, 2026-08-18, gepatchte Binary)

Durchgehende Stall-Last statt Einzelschuss, damit das `P` kein 10-Sekunden-Fenster
treffen muss: 4 gestaffelte Verbindungen, jede pipelined 150 Requests, liest nie
und erneuert sich alle 15 s (`/tmp/stallhold.py` auf mvsdev). Wirkung sichtbar an
der Antwortzeit: httpd braucht unter Last 0,95 s statt 0,07 s — die Worker
stecken messbar in Stall-Pausen. Dann `P HTTPD`:

```
12:50:23  P HTTPD
12:50:23  HTTPD098I HTTPD SHUTTING DOWN        <-- dieselbe Sekunde
12:50:26  IEF404I HTTPD - ENDED - TIME=05.50.26
```

Port 8080 war nach **1,12 s** zu, der Adressraum nach **3 s** beendet — also auf
dem Niveau eines Stops ohne jede Last (Baseline 1,22 s). Kein `C HTTPD` nötig,
kein wedged Worker, Hercules unbeeindruckt. Zum Vergleich das Verhalten davor:
Der Adressraum blieb stehen, nur `C HTTPD` half.

Ohne die Quiesce-Prüfung hätte jeder blockierte Worker sein Restbudget
ausgesessen (bis 10 s pro Worker, bei sich erneuernden Stalls entsprechend
länger). Die Prüfung greift also innerhalb einer `SEND_STALL_PAUSE`, wie in
PR #206 zugesagt.

### #206-Nachweis — Durchführung

Auf dem gepatchten Emulator lässt sich endlich Test B fahren: Stall-Client starten,
in das ~10-Sekunden-Stall-Fenster hinein `P HTTPD` absetzen (Konsole/tmux). GREIN
für #206 = die STC kommt innerhalb von ~100 ms (einer `SEND_STALL_PAUSE`) herunter,
statt das volle Stall-Budget auszusitzen.

---

## 6. gdb-Falle (optional — Backtrace beim Crash einsammeln)

Nur nötig, wenn ein frischer Backtrace für den Upstream-Report gebraucht wird.
Nach `setcap -r` (Abschnitt 3) ohne sudo möglich.

`~/trap.gdb`:

```
set pagination off
set confirm off
handle SIGPIPE nostop noprint pass
handle SIGUSR1 nostop noprint pass
handle SIGUSR2 nostop noprint pass
handle SIGWINCH nostop noprint pass
handle SIGHUP  nostop noprint pass
handle SIGCONT nostop noprint pass
set logging file /home/mike/MVSCE/herc-crash-bt.txt
set logging overwrite on
set logging enabled on
echo === GDB ARMED ===\n
continue
echo === PROCESS STOPPED ===\n
info program
x/8i $pc
info registers
info threads
thread apply all bt
echo === FULL BACKTRACE FAULTING THREAD ===\n
bt full
detach
quit
```

Armieren (eigenes tmux-Fenster, blockiert bis zum Crash):

```sh
ssh mvsdev.lan 'tmux new-window -d -t 0: -a -n gdbtrap \
  "gdb -q -p $(pgrep -f \"hercules -f\") -x ~/trap.gdb; read"'
# ... dann Repro aus Schritt 3 ...
ssh mvsdev.lan 'grep -nE "^Thread|tcpip.c|x75.c|impl.c" ~/MVSCE/herc-crash-bt.txt'
```

Post-mortem-Alternative (falls Core geschrieben wurde):
`gdb /usr/local/hercules/bin/hercules ~/MVSCE/core -batch -ex 'thread apply all bt'`

---

## 7. Recovery

```sh
# in der tmux-Session 0 auf mvsdev:
./start_mvs.sh
# bzw. per Webkonsole/tmux, bis der IPL durch ist und HTTPD/UFSD/FTPD stehen.
```

Merke: `httpd->mgr`- und `WORKERS`-Adressen ändern sich pro IPL nicht garantiert,
aber prüfe sie nach jedem Neustart neu (`/.dsrv?target=HTTPD` → `httpd->mgr`,
dann `/.dsrv?target=MGR&m=<mgr>` → `WORKERS&m=<addr>`).

---

## 8. Warnungen

- **Jeder RED-Lauf tötet den Emulator.** Nur auf der Dev-Maschine, nie gegen ein
  System mit laufender fremder Arbeit. IPL-Zeit einplanen.
- Der Fehler ist **nicht httpd-spezifisch** — jedes Gast-`send()` über libc370-
  Sockets (ftpd, mvsMF, …) kann ihn auslösen. httpd ist nur der bequemste Trigger.
- Solange `tcpip.c` ungepatcht ist: **keine Stalled-Socket-Lasttests** im Alltag.
- Die httpd-Stall-Logik (#200/#202/#206) ist auf dieser Plattform bis zum
  Hercules-Patch praktisch toter Code — aber **nicht entfernen**: Sie ist die
  Gast-Hälfte des Fixes. Ein gepatchter Emulator gegen ein Prä-#200-httpd würde
  den #199-Spin erneut auslösen.

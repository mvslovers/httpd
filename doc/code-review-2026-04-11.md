# Code Review

Datum: 2026-04-11

Fokus:
- Memory-Safety
- Security
- C99/gnu99-Kontext
- Speicherrestriktionen auf 24-Bit-MVS

## Executive Summary

Es wurden sieben relevante Findings identifiziert:

- 5 x `P1`
- 2 x `P2`

Die kritischsten Punkte sind mehrere remote erreichbare Stack-Overflows in request-nahen Pfaden sowie ein fehlerhafter Nonblocking-Sendepfad, der Worker festfahren oder HTTP/1.1-Chunking beschädigen kann.

## Findings

### 1. [P1] Long directory URI overflows stack buffer

Datei: `src/httpget.c:46-56`

`REQUEST_PATH` ist client-kontrolliert und kann sich der Größe von `CBUFSIZE` annähern. `httpget()` kopiert den Pfad in `buf[300]` und hängt danach `index.html` oder `default.html` ohne Bounds-Check an. Ein langer Verzeichnis-URI überschreibt damit den Stack, noch bevor die Datei geöffnet wird.

Betroffene Stelle:

```c
/* If the path is for a directory, try index.html / default.html */
len = strlen(path);
if (path[len-1]=='/') {
    memcpy(buf, path, len);
    strcpy(&buf[len], "index.html");
    mime = http_mime(buf);
    open_path = buf;
    fp = http_open(httpc, buf, mime);
    if (fp || httpc->ufp) goto okay;

    strcpy(&buf[len], "default.html");
    mime = http_mime(buf);
    open_path = buf;
    fp = http_open(httpc, buf, mime);
    if (fp || httpc->ufp) goto okay;
}
```

Auswirkung:
- Remote Stack-Overflow
- Absturz oder potenziell kontrollierbare Speicherbeschädigung

Empfehlung:
- Pfadlänge vor dem Kopieren strikt prüfen
- `snprintf()` oder explizite Längenberechnung verwenden
- Idealerweise denselben Maximalwert zentral für Request-Pfade und Pfadableitungen erzwingen

### 2. [P1] Cookie value is formatted into fixed 512-byte stack buffer

Datei: `src/httpcred.c:253-255`

`Sec-Uri` stammt aus einem Request-Cookie. `print_body()` formatiert den Wert per `sprintf()` in `buf[512]`. Ein langer Cookie kann den Stack überschreiben. Zusätzlich wird der Wert ungefiltert in ein HTML-Attribut geschrieben, wodurch reflektierte Markup-/XSS-Injection möglich wird.

Betroffene Stelle:

```c
for(i=0; body3[i]; i++) {
    sprintf(buf, body3[i], uri);
    if (rc=http_printf(httpc, " %s\n", buf)) goto quit;
}
```

Auswirkung:
- Remote Stack-Overflow
- HTML-/Attribut-Injection im Login-Formular

Empfehlung:
- `snprintf()` statt `sprintf()`
- `uri` vor HTML-Ausgabe escapen
- Cookie-Länge serverseitig begrenzen

### 3. [P1] Lua CGI fallback copies request path with strcpy

Datei: `src/httplua.c:460-464`

Wenn die Lua-Script-Auflösung auf `REQUEST_PATH` zurückfällt, wird der komplette Request-Pfad mit `strcpy()` nach `dataset[256]` kopiert. Der Request-Line-Pfad darf aktuell deutlich länger sein. Das ist ein remote auslösbarer Stack-Overflow.

Betroffene Stelle:

```c
/* last chance, see if the request path exist */
script = http_get_env(httpc, "REQUEST_PATH");
if (script) {
    strcpy(dataset, script);
    if (readable(dataset))
        goto doit;
}
```

Auswirkung:
- Remote Stack-Overflow

Empfehlung:
- Hartes Längenlimit vor dem Fallback
- `snprintf()` oder sichere Kopierlogik verwenden
- Besser: Request-Pfad nicht direkt als lokaler Dataset-/Dateiname verwenden

### 4. [P1] Decoded Sec-Uri can overrun header construction

Datei: `src/httpcred.c:392-435`

`base64_decode()` kann beliebige Bytefolgen zurückgeben. `strncpy()` nach `uribuf[256]` garantiert keine abschließende NUL-Terminierung. Der Wert wird danach in Vergleichen und im `Location:`-Header verwendet. Das kann zu Out-of-Bounds-Reads führen. Zusätzlich erlaubt der ungefilterte Redirect-Wert CR/LF-basierte Header-Injection.

Betroffene Stelle:

```c
if (uri) {
    buf = base64_decode(uri, strlen(uri), &len);
    if (buf) {
        strncpy(uribuf, buf, sizeof(uribuf));
        free(buf);
        uri = uribuf;
    }

    if (http_cmpn(uri, "/login", 6)==0) {
        uri = "/";
    }
}

...

http_printf(httpc, "Location: %s\r\n", uri);
```

Auswirkung:
- Out-of-bounds read
- Header-Injection / Response-Splitting
- Unsaubere Redirect-Ziele

Empfehlung:
- Nach `strncpy()` immer terminieren oder direkt `snprintf()` verwenden
- Dekodierte Werte auf erlaubte Redirect-Form begrenzen
- `\r` und `\n` explizit verwerfen
- Nur relative Pfade wie `/...` erlauben

### 5. [P1] Non-blocking send path can spin forever or corrupt chunked output

Datei: `src/httpsend.c:14-70`

Akzeptierte Sockets werden auf non-blocking gestellt. `send_raw()` bricht bei `EWOULDBLOCK` ab und gibt die bisher gesendete Bytezahl zurück, die auch `0` sein kann. Aufrufer wie `httpprtv()` inkrementieren `pos` mit diesem Rückgabewert in einer Schleife. Dadurch kann ein Worker bei blockiertem Peer hängen. Im Chunked-Fall werden Header, Body und Trailer außerdem behandelt, als seien sie vollständig versendet, obwohl Short Writes aufgetreten sein können.

Betroffene Stellen:

```c
for(pos=0; pos < len; pos+=rc) {
    rc = send(httpc->socket, &buf[pos], len-pos, 0);
    if (rc>0) {
        httpc->sent += rc;
    }
    else {
        if (errno == EWOULDBLOCK) break;
        ...
    }
}
```

und indirekt:

```c
for (pos = 0; pos < te_len; pos += rc) {
    rc = http_send(httpc, &te[pos], te_len - pos);
    if (rc < 0) goto quit;
}
```

Auswirkung:
- Worker kann in Busy-/Endlosschleife geraten
- Chunked Responses können protokollseitig beschädigt werden
- Schlechter Durchsatz unter Last oder bei langsamen Clients

Empfehlung:
- Klare Semantik für `http_send()`: vollständiger Versand, `EWOULDBLOCK`, oder Fehler
- Partials sauber im Client-State puffern
- Chunk-Header, Chunk-Body und Trailer atomar über Zustandsmaschine senden

### 6. [P2] Percent decoder reads past end on malformed escapes

Datei: `src/httpdeco.c:15-21`

`http_decode()` liest `str[1]` und `str[2]`, bevor geprüft wird, ob diese Bytes überhaupt existieren. Ein URI, der mit `%` oder `%x` endet, führt zu einem Out-of-Bounds-Read. Ungültige Escape-Sequenzen werden außerdem stillschweigend konsumiert statt sauber als Bad Request verworfen.

Betroffene Stelle:

```c
case '%':
    temp[0] = str[1];
    temp[1] = str[2];
    temp[2] = 0;
    out[0] = asc2ebc[strtoul(temp, NULL, 16)];
    if (str[1] && str[2]) str+=2;
    break;
```

Auswirkung:
- Out-of-bounds read
- Fehlerhafte oder uneindeutige URI-Normalisierung

Empfehlung:
- Vor Zugriff auf `str[1]` und `str[2]` auf Vollständigkeit prüfen
- Nur echte Hex-Sequenzen dekodieren
- Ungültige Escapes mit `400 Bad Request` ablehnen

### 7. [P2] SSI formatter still uses unbounded vsprintf

Datei: `src/httpfile.c:591-605`

Der SSI-Pfad formatiert Ausgaben mit `vsprintf()` in einen Stackpuffer von 4096 Byte. SSI-Seiten können requestnahe Daten und Environment-Variablen ausgeben. Dadurch bleibt hier ein klassischer Overflow-Pfad bestehen.

Betroffene Stelle:

```c
char buf[4096];

/* Okay here we go. Format the string into the buffer */
len = vsprintf(buf, fmt, args);
if (len < 0) {
    ...
}
else {
    ssi_buffer(httpc, buf, len);
}
```

Auswirkung:
- Stack-Overflow im SSI-Ausgabepfad

Empfehlung:
- `vsnprintf()` verwenden
- Bei Trunkierung klar reagieren
- Optional SSI-Ausgaben stückweise puffern statt großem Stackbuffer

## Zusätzliche Hinweise

### Sprachstandard

In `config.mk` wird aktuell kein expliziter Sprachstandard wie `-std=c99` oder `-std=gnu99` erzwungen:

```make
CC       := c2asm370
CFLAGS   := -fverbose-asm -S -O1
```

Wenn C99/gnu99 der Zielstandard sein soll, sollte das im Build explizit gesetzt werden, soweit der Cross-Compiler dies unterstützt.

### Hauptspeicher / 24-Bit-MVS

Für die Zielplattform ist die Speicherökonomie wichtig. Neben den Sicherheitsfunden fällt auf:

- `__stklen = 64*1024`
- Socket-Thread mit `32*1024`
- Worker-Threads mit bis zu `64*1024` Stack je Thread
- mehrere zusätzliche Stackpuffer im Request-Pfad (`512`, `300`, `256`, `4096`, `5120` Bytes)

Das ist nicht automatisch falsch, sollte aber im Kontext der 24-Bit-Mainstorage-Grenzen bewusst beobachtet werden.

## Empfohlene Priorisierung

1. Alle remote erreichbaren Stack-Overflows beseitigen (`httpget.c`, `httpcred.c`, `httplua.c`, `httpfile.c`)
2. Redirect-/Header-Injection in `httpcred.c` schließen
3. Nonblocking-Sendelogik korrekt zustandsbasiert machen
4. URI-Decoder gegen unvollständige `%`-Sequenzen härten
5. Danach Memory-Footprint der Hot Paths weiter reduzieren

## Methodik

Die Findings basieren auf statischer Analyse des aktuellen Quellstands im Repository. Es wurden in diesem Schritt keine Builds oder Laufzeittests durchgeführt.

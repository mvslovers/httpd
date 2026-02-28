/* HTTPREPO.C
** Report results of request
** Transitions to next client state.
*/
#include "httpd.h"

static char * FmtStats(HTTPC *httpc, char *buf, int size);
#define STATS_FMT   "%h %l %u %t \"%r\" %>s %b %T"

extern int
httprepo(HTTPC *httpc)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = 0;
    int         lock;
    char        buf[2048];

    http_enter("httprepo()\n");

	/* If we're configured for client statistics */
	if (httpd->client & HTTPD_CLIENT_STATS) {
		/* Add this client to the statistics records */
		httpstat_add(httpd, httpc); 
	}

    /* are we recording statistics? */
    if (!httpd->stats) goto quit;   /* nope, we're done */

    FmtStats(httpc, buf, sizeof(buf));

    fprintf(httpd->stats, "%s\n", buf);

quit:
    /* transition to next state */
    httpc->state = CSTATE_RESET;
    http_exit("httprepo(), rc=%d\n", rc);
    return rc;
}

__asm__("\n&FUNC    SETC 'FmtStats'");
static char *
FmtStats(HTTPC *httpc, char *buf, int size)
{
    CLIBGRT             *grt    = __grtget();
    HTTPD               *httpd  = grt->grtapp1;
    const char          *fmt    = STATS_FMT;
    unsigned            pos     = 0;
    unsigned            remain;
    int                 addrlen;
    struct sockaddr_in  *in;
    unsigned            not;
    unsigned            last;
    const char          *p;
    unsigned            len;
    char                key[256];
    struct sockaddr     sockaddr;
    struct in_addr      in_addr;

    buf[0] = 0;
    size -= 80; /* leave room at end of buffer */

    while (*fmt) {
        if (pos > size) break;
        not     = 0;
        last    = 0;
        key[0]  = 0;

        if (*fmt == '\\') {
            fmt++;
            switch(*fmt) {
            case 'r':   // \r --> CR
                buf[pos++] = '\r';
                break;
            case 'n':   // \n --> NL
                buf[pos++] = '\n';
                break;
            case '"':   // \" --> "
                buf[pos++] = '"';
                break;
            case '\'':  // \' --> '
                buf[pos++] = '\'';
                break;
            case '\\':  // \\ --> \
                buf[pos++] = '\\';
                break;
            default :
                buf[pos++] = '\\';
                buf[pos++] = *fmt;
                break;
            }
            fmt++;
            continue;
        }

        if (*fmt != '%') {
            /* Not a formatting directive, copy asis */
            buf[pos++] = *fmt++;
            continue;
        }

        /* % format directives */
        fmt++;

        /* check for negation */
        if (*fmt=='!') {
            not = 1;
            fmt++;
        }

        /* skip resp code conditionals */
        while(isdigit(*fmt)||*fmt==',') fmt++;

        if (*fmt=='>') {
            last = 1;
            fmt++;
        }

        /* Check for {key} */
        if (*fmt=='{') {
            fmt++;
            len = 0;
            while(*fmt && *fmt != '}') {
                key[len++] = *fmt++;
            }
            key[len] = 0;
            fmt++;
        }

        switch(*fmt) {
        case '%':
            buf[pos++] = *fmt;
            break;

        case 'A':   /* A: Local IP-address */
            addrlen = sizeof(sockaddr);
            if (getsockname(httpc->socket, &sockaddr, &addrlen)==0) {
                in = (struct sockaddr_in*)&sockaddr;
                p = http_ntoa(in->sin_addr);
                len = p ? strlen(p) : 0;

                if (p) {
                    remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
                    if (len > remain) len = remain;
                    memcpy(&buf[pos], p, len);
                    pos += len;
                }
                else {
                    buf[pos++] = '-';
                }
            }
            else {
                buf[pos++] = '-';
            }
            break;

        case 'B':   /* B: Bytes sent. */
            remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
            pos += snprintf( &buf[pos], remain, "%u", httpc->sent );
            break;

        case 'b':   /* b: Bytes sent. In CLF format
                    ** i.e. a '-' rather than a 0 when no bytes are sent.
                    */
            if (httpc->sent) {
                remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
                pos += snprintf( &buf[pos], remain, "%u", httpc->sent );
            }
            else {
                buf[pos++] = '-';
            }
            break;

        case 'c':
            /* c: Connection status when response was completed.
            **    'X' = connection aborted before the response completed.
            **    '+' = connection may be kept alive after the response is sent.
            **    '-' = connection will be closed after the response is sent.
            ** we're always closing the connection after the response
            */
            buf[pos++] = '-';
            break;

        case 'e':   /* e: The contents of the environment variable */
            p = http_get_env(httpc, key);
            if (p) {
                len = strlen(p);
                remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
                if (len > remain) len = remain;
                memcpy(&buf[pos], p, len);
                pos += len;
            }
            else {
                buf[pos++] = '-';
            }
            break;

        case 'f':   /* f: Filename */
            p = http_get_env(httpc, "REQUEST_PATH");
            if (p) p = strrchr(p, '/' );
            if (p) {
                p++;
                len = strlen(p);
                remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
                if (len > remain) len = remain;
                memcpy(&buf[pos], p, len);
                pos += len;
            }
            else {
                buf[pos++] = '-';
            }
            break;

        case 'h':   /* h: Remote host */
#if 0
            buf[pos++] = '-';
            break;
#else
            in_addr.s_addr = httpc->addr;
            p = http_ntoa(in_addr);
            len = p ? strlen(p) : 0;

            if (p) {
                remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
                if (len > remain) len = remain;
                memcpy(&buf[pos], p, len);
                pos += len;
            }
            else {
                buf[pos++] = '-';
            }
            break;
#endif
        case 'H':   /* H:   The request protocol */
            remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
            if (4 <= remain) {
                memcpy(&buf[pos], "HTTP", 4);
                pos += 4;
            }
            break;

        case 'i':   /* i:  The contents of {key}: header line(s) in the request
                    **     sent to the server.
                    */
            buf[pos++] = '-';
            break;

        case 'I':
            buf[pos++] = '-';
            break;

        case 'l':   /* l: Remote logname (from identd, if supplied) */
            buf[pos++] = '-';
            break;

        case 'm':   /* m: The request method */
            p = http_get_env(httpc, "REQUEST_METHOD");
            if (p) {
                len = strlen(p);
                remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
                if (len > remain) len = remain;
                memcpy(&buf[pos], p, len);
                pos += len;
            }
            else {
                buf[pos++] = '-';
            }
            break;

        case 'n':   /* n: The contents of note {key} from another module. */
            buf[pos++] = '-';
            break;

        case 'o':   /* o: The contents of {key}: header line(s) in the reply. */
            buf[pos++] = '-';   /* not implemented */
            break;

        case 'O':
            remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
            pos += snprintf(&buf[pos], remain, "%u", httpc->sent);
            break;

        case 'p':   /* p: The canonical Port of the server serving the request
                    */
            remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
            pos += snprintf(&buf[pos], remain, "%d", httpd->port );
            break;

        case 'P':   /* P: The process ID of the child that serviced the request.
                    */
            buf[pos++] = '-';
            break;

        case 'q':   /* q: The query string
                    **    (prepended with a ? if a query string exists,
                    **    otherwise an empty string)
                    */
            p = http_get_env(httpc, "QUERY_STRING");
            if (p) {
                remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
                pos += snprintf(&buf[pos], remain, "?%s", p);
            }
            break;

        case 'r':   /* r: First line of request */
            p = http_get_env(httpc, "HTTP_REQUEST");
            if (p) {
                len = 0;
                while(*p && !iscntrl(p[len])) len++;
                remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
                if (len > remain) len = remain;
                memcpy(&buf[pos], p, len);
                pos += len;
            }
            else {
                buf[pos++] = '-';
            }
            break;

        case 's':   /* s: Status.  For requests that got internally redirected,
                    **    this is the status of the *original* request
                    **    --- %...>s for the last.
                    */
            if (httpc->resp) {
                remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
                pos += snprintf( &buf[pos], remain, "%d", httpc->resp );
            }
            else {
                buf[pos++] = '-';
            }
            break;

        case 't':   /* t: Time, in common log format time format
                    ** {format}t:  The time, in the form given by format
                    */
            remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
            if (key[0]) {
                time64_t t = time64(NULL);
                pos += strftime(&buf[pos], remain, key, localtime64(&t));
            }
            else {
                http_date_rfc1123(time64(NULL), &buf[pos], remain);
                pos += strlen(&buf[pos]);
            }
            break;

        case 'T':   /* T: The time taken to serve the request, in seconds. */
            remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
            pos += snprintf(&buf[pos], remain, "%f", httpc->end - httpc->start);
            break;

        case 'u':   /* u: Remote user (from auth;
                    **    may be bogus if return status (%s) is 401)
                    */
            p = http_get_env(httpc, "REMOTE_USER");
            if (p) {
                len = strlen(p);
                remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
                if (len > remain) len = remain;
                memcpy(&buf[pos], p, len);
                pos += len;
            }
            else {
                buf[pos++] = '-';
            }
            break;

        case 'U':   /* U: The URL path requested,
                    **    not including any query string.
                    */
            p = http_get_env(httpc, "REQUEST_PATH");
            if (p) {
                len = strlen(p);
                remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
                if (len > remain) len = remain;
                memcpy(&buf[pos], p, len);
                pos += len;
            }
            else {
                buf[pos++] = '-';
            }
            break;

        case 'v':   /* v: The canonical ServerName of the server
                    **    serving the request.
                    */
        case 'V':   /* V: The server name according to the
                    **    UseCanonicalName setting.
                    */
            p = http_get_env(httpc, "SERVER_NAME");
            if (p) {
                len = strlen(p);
                remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
                if (len > remain) len = remain;
                memcpy(&buf[pos], p, len);
                pos += len;
            }
            else {
                buf[pos++] = '-';
            }
            break;

        default:
            buf[pos++] = '%';
            if (key[0]) {
                remain = (pos < (unsigned)size) ? (unsigned)size - pos : 0;
                pos += snprintf(&buf[pos], remain, "{%s}", key );
            }
            buf[pos++] = *fmt;
            break;
        }
        fmt++;
    }

    buf[pos] = 0;

quit:
    return buf;
}

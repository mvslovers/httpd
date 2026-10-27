/* TSTSSIF.C
** Tests for the SSI output of httpfile.c -- issue #401.
**
** A page is served through http_send_file() with SSI on, and what reaches
** http_send() is compared with what the page should become:
**
**   - <!--#printenv --> lists the variables of the request and nothing of
**     the server's own environment (no "System Variables" table);
**   - the values printenv and <!--#echo --> write are HTML text: markup in
**     a header comes out as entities, not as markup;
**   - a line of any length is written as one line, and a directive that
**     runs across the point where the line is read in pieces is still
**     recognised.
**
** The page is a UFSFILE whose read-ahead buffer holds the text and whose
** EOF flag stops ufs_fgets() from asking UFSD for more, as in TSTFILE.
** The test brings its own HTTPX vector with the entries httpfile.c touches
** on these paths.  Needs httpd.h, so MVS-only (mbt test --mvs); the pure
** parts are in TSTSSI, which also runs on the host.
**
** HTTP_PRIVATE selects the real HTTPFILE entry (called directly).
*/
#define HTTP_PRIVATE
#include "httpd.h"
#include <ctype.h>
#include <mbtcheck.h>

#define OUTMAX  8192

static char     out[OUTMAX];
static int      outlen;
static HTTPD    fakehttpd;
static HTTPCP   fakecp;
static HTTPX    fake;

HTTPX *httpx = &fake;

static int
fake_send(HTTPC *httpc, const UCHAR *buf, int len)
{
    (void)httpc;
    if (outlen + len < OUTMAX) {
        memcpy(&out[outlen], buf, len);
        outlen += len;
        out[outlen] = 0;
    }
    return len;
}

static int
fake_cmp(const UCHAR *a, const UCHAR *b)
{
    while (*a && tolower(*a) == tolower(*b)) {
        a++;
        b++;
    }
    return tolower(*a) - tolower(*b);
}

static unsigned char *
fake_xlate(unsigned char *buf, int len, const unsigned char *tbl)
{
    (void)len;
    (void)tbl;
    return buf;
}

/* the request variables the test put into httpc->env */
static UCHAR *
fake_get_env(HTTPC *httpc, const UCHAR *name)
{
    unsigned n;
    unsigned count = array_count(&httpc->env);

    for (n = 0; n < count; n++) {
        if (strcmp(httpc->env[n]->name, (const char *)name) == 0)
            return (UCHAR *)httpc->env[n]->value;
    }
    return NULL;
}

static void
setvar(HTTPC *httpc, const char *name, const char *value)
{
    HTTPV *v = calloc(1, sizeof(HTTPV) + strlen(name) + strlen(value) + 2);

    strcpy(v->name, name);
    v->value = &v->name[strlen(name) + 1];
    strcpy(v->value, value);
    array_add(&httpc->env, v);
}

/* serve page as an SSI page, the result is in out[] */
static void
serve(HTTPC *httpc, const char *page)
{
    UFSFILE *fp = calloc(1, sizeof(UFSFILE));
    int     pass;

    memcpy(fp->eye, "LIBUFSFP", 8);
    fp->fd       = 3;
    fp->flags    = LIBUFS_F_EOF;
    fp->rbuf_len = strlen(page);
    memcpy(fp->rbuf, page, fp->rbuf_len);

    httpc->state    = CSTATE_GET;
    httpc->substate = 0;
    httpc->chunked  = 0;
    httpc->httpd    = &fakehttpd;
    httpc->ufp      = fp;
    httpc->ssi      = 1;
    httpc->len      = 0;
    httpc->pos      = 0;
    outlen          = 0;
    out[0]          = 0;

    for (pass = 0; pass < 2000 && httpc->state != CSTATE_DONE; pass++) {
        http_send_file(httpc, 0);
    }
    free(fp);
}

static int
is(const char *want)
{
    return strcmp(out, want) == 0;
}

int main(void)
{
    HTTPC   *httpc;
    char    page[1400];
    char    want[1400];
    int     pad;
    int     bad;
    int     n;

    fake.http_get_env = fake_get_env;
    fake.http_send    = fake_send;
    fake.http_cmp     = fake_cmp;
    fake.http_xlate   = fake_xlate;
    fake.array_count  = (unsigned (*)(void *))array_count;
    fake.xlate_1047   = &fakecp;

    httpc = calloc(1, sizeof(HTTPC));
    if (!httpc) {
        printf("TSTSSIF: no storage for HTTPC\n");
        return 8;
    }
    httpc->env = array_new(16);
    setvar(httpc, "REQUEST_METHOD", "GET");
    setvar(httpc, "HTTP_HOST", "example");
    setvar(httpc, "HTTP_USER_AGENT", "<script>alert(1)</script>");
    setvar(httpc, "HTTP_REFERER", "\"><b>x</b> & 'q'");
    setenv("MVSSECRET", "hunter2", 1);

    printf("=== HTTPD SSI output tests ===\n");

    /* printenv: the request, not the server */
    serve(httpc, "<!--#printenv -->\n");
    printf("  printenv: %d bytes\n", outlen);
    CHECK(strstr(out, "REQUEST_METHOD") != NULL,
          "printenv lists the request variables");
    CHECK(strstr(out, "System Variables") == NULL,
          "printenv has no System Variables table");
    CHECK(strstr(out, "MVSSECRET") == NULL,
          "printenv does not name a server environment variable");
    CHECK(strstr(out, "hunter2") == NULL,
          "printenv does not show a server environment value");

    /* printenv and echo write values as HTML text */
    CHECK(strstr(out, "<script>") == NULL,
          "printenv: a header value is not markup");
    CHECK(strstr(out, "&lt;script&gt;alert(1)&lt;/script&gt;") != NULL,
          "printenv: a header value is escaped");

    serve(httpc, "<p><!--#echo var=\"HTTP_REFERER\" --></p>\n");
    CHECK(is("<p>&quot;&gt;&lt;b&gt;x&lt;/b&gt; &amp; &#39;q&#39;</p>\n"),
          "echo: a header value is escaped");

    serve(httpc, "a<!--#echo var=\"HTTP_HOST\" -->b\n");
    CHECK(is("aexampleb\n"), "echo: a plain value is unchanged");

    /* a line of any length is one line */
    memset(page, 'a', 600);
    strcpy(&page[600], "\nnext\n");
    memset(want, 'a', 600);
    strcpy(&want[600], "\nnext\n");
    serve(httpc, page);
    CHECK(is(want), "a 600 character line stays one line");

    memset(page, 'b', 511);
    strcpy(&page[511], "\n");
    serve(httpc, page);
    CHECK(strlen(out) == 512 && out[511] == '\n',
          "a line of the read size gets one newline");

    memset(page, 'c', 1300);
    strcpy(&page[1300], "\n");
    serve(httpc, page);
    CHECK(strlen(out) == 1301 && strchr(out, '\n') == &out[1300],
          "a 1300 character line gets one newline, at its end");

    /* the last line of a file may lack its newline; it gets one */
    serve(httpc, "last");
    CHECK(is("last\n"), "an unterminated last line gets its newline");

    /* a directive wherever the line is cut stays a directive */
    bad = 0;
    for (pad = 440; pad < 520; pad++) {
        memset(page, 'x', pad);
        strcpy(&page[pad], "<!--#echo var=\"HTTP_HOST\" -->tail\n");
        memset(want, 'x', pad);
        strcpy(&want[pad], "exampletail\n");
        serve(httpc, page);
        if (!is(want)) {
            if (!bad) printf("  directive after %d bytes: not recognised\n", pad);
            bad++;
        }
    }
    CHECK_EQ(bad, 0, "a directive is found wherever the line is cut");

    /* the same for a directive at the end of a line of text */
    n = 0;
    for (pad = 440; pad < 520; pad += 7) {
        memset(page, 'y', pad);
        strcpy(&page[pad], "<!--#echo var=\"HTTP_HOST\" -->\n");
        memset(want, 'y', pad);
        strcpy(&want[pad], "example");
        serve(httpc, page);
        if (!is(want)) n++;
    }
    CHECK_EQ(n, 0, "a directive that ends a long line is found");

    free(httpc);
    return mbt_test_summary("TSTSSIF");
}

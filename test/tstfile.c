/* TSTFILE.C
** Tests for http_send_file() / httpfile.c when UFS fails a read -- issue #263.
**
** http_send_file() asked ufs_feof() only.  A failed ufs_fread() sets the
** error flag and returns 0 with EOF unset, so nothing ever moved the state on:
** the worker went round until serve_client() gave up after 1000 passes and
** wrote HTTPD901E, blaming the client for a fault of the filesystem.  A read
** error must instead end the response on the first pass:
**
**   - the state goes to CSTATE_DONE, so the send loop ends;
**   - chunk framing is switched off, so httpdone() writes no terminating
**     "0" chunk -- the one thing that tells the client the body is torn;
**   - keep-alive is off, so the connection closes behind it;
**   - nothing still in the buffer reaches the socket.
**
** A UFSFILE whose error flag is set stands in for the failed read: ufs_fread()
** and ufs_fgetc() return at once on it, without a UFSD.  A file that merely
** reached EOF is the control -- it must keep its chunk terminator and its
** connection.
**
** The test brings its own HTTPX vector (httpx.c would drag in the whole
** server) with the two entries httpfile.c touches on these paths.  Needs
** httpd.h, so MVS-only (mbt test --mvs).  HTTPFILE resolves from the
** [internal] autocall archive.
**
** HTTP_PRIVATE selects the real HTTPFILE entry (called directly).
*/
#define HTTP_PRIVATE
#include "httpd.h"
#include <mbtcheck.h>

static int sent_calls;

static UCHAR *
fake_get_env(HTTPC *httpc, const UCHAR *name)
{
    (void)httpc;
    (void)name;
    return (UCHAR *)"/docs/index.html";
}

static int
fake_send(HTTPC *httpc, const UCHAR *buf, int len)
{
    (void)httpc;
    (void)buf;
    (void)len;
    sent_calls++;
    return len;
}

static HTTPX fake;

HTTPX *httpx = &fake;

/* the state of a response in flight: chunked, kept alive, bytes still unsent */
static void
streaming(HTTPC *httpc, UFSFILE *ufp, int ssi)
{
    memset(httpc, 0, sizeof(HTTPC));
    httpc->state     = CSTATE_GET;
    httpc->chunked   = 1;
    httpc->keepalive = 1;
    httpc->ufp       = ufp;
    httpc->ssi       = (UCHAR)ssi;
    memcpy(httpc->buf, "unsent", 6);
    httpc->len       = 6;
    httpc->pos       = 0;
    sent_calls       = 0;
}

static UFSFILE *
newfile(unsigned flags, int error)
{
    UFSFILE *fp = calloc(1, sizeof(UFSFILE));

    if (fp) {
        memcpy(fp->eye, "LIBUFSFP", 8);
        fp->fd    = 3;
        fp->flags = flags;
        fp->error = error;
    }
    return fp;
}

static void
check_aborted(HTTPC *httpc, const char *what)
{
    printf("  %s: state=%d chunked=%d keepalive=%d len=%d sent=%d\n", what,
        httpc->state, httpc->chunked, httpc->keepalive, httpc->len,
        sent_calls);
    CHECK_EQ(httpc->state, CSTATE_DONE, "read error ends the send loop");
    CHECK_EQ(httpc->chunked, 0, "read error drops chunk framing");
    CHECK_EQ(httpc->keepalive, 0, "read error closes the connection");
    CHECK_EQ(httpc->len, 0, "read error discards the unsent buffer");
    CHECK_EQ(sent_calls, 0, "read error sends nothing more");
}

int main(void)
{
    HTTPC   *httpc;
    UFSFILE *ufp;

    fake.http_get_env = fake_get_env;
    fake.http_send    = fake_send;

    httpc = calloc(1, sizeof(HTTPC));
    if (!httpc) {
        printf("TSTFILE: no storage for HTTPC\n");
        return 8;
    }

    printf("=== HTTPD http_send_file UFS read error tests ===\n");

    /* binary body, as served for images and the like */
    ufp = newfile(LIBUFS_F_ERR, UFSD_RC_IO);
    streaming(httpc, ufp, 0);
    http_send_file(httpc, 1);
    check_aborted(httpc, "binary");
    free(ufp);

    /* text body */
    ufp = newfile(LIBUFS_F_ERR, UFSD_RC_IO);
    streaming(httpc, ufp, 0);
    http_send_file(httpc, 0);
    check_aborted(httpc, "text");
    free(ufp);

    /* server-parsed (.html) body, read line by line */
    ufp = newfile(LIBUFS_F_ERR, UFSD_RC_IO);
    streaming(httpc, ufp, 1);
    http_send_file(httpc, 0);
    check_aborted(httpc, "ssi");
    free(ufp);

    /* control: plain EOF is not an error -- the chunk terminator and the
    ** connection stay, and the unsent buffer is still delivered first */
    ufp = newfile(LIBUFS_F_EOF, 0);
    streaming(httpc, ufp, 0);
    http_send_file(httpc, 1);
    printf("  eof: state=%d chunked=%d keepalive=%d len=%d sent=%d\n",
        httpc->state, httpc->chunked, httpc->keepalive, httpc->len,
        sent_calls);
    CHECK_EQ(httpc->state, CSTATE_DONE, "EOF ends the send loop");
    CHECK_EQ(httpc->chunked, 1, "EOF keeps the chunk terminator");
    CHECK_EQ(httpc->keepalive, 1, "EOF keeps the connection");
    CHECK_EQ(sent_calls, 1, "EOF still delivers the buffered bytes");
    free(ufp);

    free(httpc);
    return mbt_test_summary("TSTFILE");
}

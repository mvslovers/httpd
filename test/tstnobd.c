/* TSTNOBD.C
** Tests for httpnobd() -- the "does this response carry a message body"
** decision (src/httpnobd.c) that httpprtv() uses to decide whether to inject
** Transfer-Encoding: chunked (issue #399).
**
** httpnobd() is free of httpd.h, so this is a DUAL test: it runs natively via
** `mbt test` and on MVS via `mbt test --mvs`.
**
** The regression this exists for: a response to HEAD was framed as chunked
** and carried a body.  A client reads no body after HEAD, so on a keep-alive
** connection the chunks became the start of the next response.
*/
#include "httpnobd.h"
#include <mbtcheck.h>

int main(void)
{
    int code;
    int bad = 0;

    /* -- HEAD: no body, whatever the status ----------------------------- */
    CHECK(httpnobd(1, 200), "HEAD 200 has no body");
    CHECK(httpnobd(1, 404), "HEAD 404 has no body");
    CHECK(httpnobd(1, 401), "HEAD 401 has no body");
    CHECK(httpnobd(1, 501), "HEAD 501 has no body");
    CHECK(httpnobd(1, 500), "HEAD 500 has no body");

    /* -- the status codes that never carry a body (RFC 7230 3.3.3) ------ */
    CHECK(httpnobd(0, 204), "204 No Content has no body");
    CHECK(httpnobd(0, 304), "304 Not Modified has no body");
    CHECK(httpnobd(0, 100), "100 has no body");
    CHECK(httpnobd(0, 101), "101 has no body");
    CHECK(httpnobd(0, 199), "199 has no body");

    /* -- everything else keeps its body for a method other than HEAD --- */
    CHECK(!httpnobd(0, 200), "GET 200 has a body");
    CHECK(!httpnobd(0, 201), "201 has a body");
    CHECK(!httpnobd(0, 206), "206 has a body");
    CHECK(!httpnobd(0, 302), "302 has a body");
    CHECK(!httpnobd(0, 400), "400 has a body");
    CHECK(!httpnobd(0, 404), "404 has a body");
    CHECK(!httpnobd(0, 500), "500 has a body");
    CHECK(!httpnobd(0, 501), "501 has a body");
    CHECK(!httpnobd(0, 200 + 0), "200 has a body");

    /* -- the edges of the 1xx range ------------------------------------- */
    CHECK(!httpnobd(0, 99),  "99 is not informational");
    CHECK(!httpnobd(0, 200), "200 is not informational");

    /* -- HEAD never has a body at all, so no code may say otherwise ----- */
    for (code = 100; code <= 599; code++) {
        if (!httpnobd(1, code)) bad++;
    }
    CHECK(bad == 0, "HEAD has no body for every status 100..599");

    return mbt_test_summary("TSTNOBD");
}

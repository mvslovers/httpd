/* TSTNENV.C
** Tests for http_new_env() / httpnenv() -- per-request env-variable block
** allocation (src/httpnenv.c).
**
** Guards the M9 sizing fix: the block is now sized with offsetof(HTTPV,name)
** instead of sizeof(HTTPV), so an off-by-one would corrupt or truncate the
** stored name/value.  These round-trip checks catch that.
**
** MVS-target (needs the HTTPV struct + the HTTPNENV entry via httpd.h), so it
** carries host = false.  HTTP_PRIVATE selects the real entry over the httpx
** vector macro.
*/
#define HTTP_PRIVATE
#include "httpd.h"
#include <mbtcheck.h>

int main(void)
{
    HTTPV   *v;

    printf("=== HTTPD http_new_env tests ===\n");

    /* name + value round-trip */
    v = http_new_env((const UCHAR *)"PATH_INFO", (const UCHAR *)"/foo/bar");
    CHECK(v != NULL, "http_new_env returns non-NULL");
    if (v) {
        CHECK(memcmp(v->eye, HTTPV_EYE, strlen(HTTPV_EYE)) == 0,
              "eye catcher is set");
        CHECK(strcmp(v->name, "PATH_INFO") == 0, "name stored intact");
        CHECK(v->value && strcmp(v->value, "/foo/bar") == 0,
              "value stored intact");
        CHECK((char *)v->value == &v->name[strlen("PATH_INFO") + 1],
              "value sits just past the name's NUL");
        free(v);
    }

    /* NULL value -> value is an empty string, not NULL */
    v = http_new_env((const UCHAR *)"EMPTY", NULL);
    CHECK(v != NULL, "NULL value returns non-NULL");
    if (v) {
        CHECK(strcmp(v->name, "EMPTY") == 0, "name stored (NULL value)");
        CHECK(v->value && v->value[0] == 0, "NULL value decodes to empty string");
        free(v);
    }

    /* empty name and empty value */
    v = http_new_env((const UCHAR *)"", (const UCHAR *)"");
    CHECK(v != NULL, "empty name/value returns non-NULL");
    if (v) {
        CHECK(v->name[0] == 0 && v->value && v->value[0] == 0,
              "empty name and value both terminated");
        free(v);
    }

    /* Both rejects return NULL, and httpsenv() tells them apart by errno to
       decide whether to report a storage shortage (HTTPD904E) or an oversized
       variable (HTTPD905E) -- issue #162.  The sanity limit is the half that
       needs no storage shortage to reach, so it is the half a test can pin. */
    {
        UCHAR *big = malloc(9000);

        CHECK(big != NULL, "oversize probe buffer allocated");
        if (big) {
            memset(big, 'X', 8999);
            big[8999] = 0;

            errno = 0;
            v = http_new_env((const UCHAR *)"BIG", big);
            CHECK(v == NULL, "name+value over the sanity limit is rejected");
            CHECK(errno == E2BIG, "oversize reject reports E2BIG, not ENOMEM");
            if (v) free(v);

            /* just under the limit still succeeds -- guards the boundary
               against the reject swallowing legitimate variables */
            big[8000] = 0;
            v = http_new_env((const UCHAR *)"BIG", big);
            CHECK(v != NULL, "name+value under the sanity limit is accepted");
            if (v) {
                CHECK(v->value && strlen(v->value) == 8000,
                      "under-limit value stored at full length");
                free(v);
            }
            free(big);
        }
    }

    return mbt_test_summary("TSTNENV");
}

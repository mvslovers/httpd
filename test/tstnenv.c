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

    return mbt_test_summary("TSTNENV");
}

/* TSTLOADX.C
** The __loadx() contract resident_hold() relies on (issue #320): a LOAD that
** fails hands back NULL and the system's completion and reason code, and a
** LOAD that works leaves both at 0.  HTTPD442W prints these two values, so a
** library that stops reporting them turns the message into "S000 REASON 00"
** for every failure.
**
** MVS-only (LOAD SVC).
*/
#include <stdio.h>
#include <string.h>
#include <mvs/link.h>
#include <mbtcheck.h>

/* a module that is not in any STEPLIB, LINKLIST or LPA */
#define NOSUCH  "NOSUCHXX"

/* a module the runner can always find: TSTLINK's LINK target */
#define PRESENT "TSTLNKM"

int main(void)
{
    void        *ep;
    unsigned    abcode;
    unsigned    reason;

    printf("=== HTTPD __loadx completion code tests (issue #320) ===\n");

    /* a missing module: NULL, completion code S806 */
    abcode = 0xDEAD;
    reason = 0xDEAD;
    ep = __loadx(NULL, NOSUCH, NULL, NULL, &abcode, &reason);
    CHECK(ep == NULL, "a missing module is not loaded");
    CHECK_EQ(abcode, 0x806, "a missing module reports S806");
    CHECK_EQ(reason, 4, "S806 carries reason code 04");

    /* a module that is there: an entry point, both codes cleared */
    abcode = 0xDEAD;
    reason = 0xDEAD;
    ep = __loadx(NULL, PRESENT, NULL, NULL, &abcode, &reason);
    CHECK(ep != NULL, "an existing module is loaded");
    CHECK_EQ(abcode, 0, "a successful LOAD leaves the completion code 0");
    CHECK_EQ(reason, 0, "a successful LOAD leaves the reason code 0");
    if (ep) CHECK_EQ(__delete(PRESENT), 0, "the loaded copy is deleted again");

    /* the out-parameters are optional */
    ep = __loadx(NULL, NOSUCH, NULL, NULL, NULL, NULL);
    CHECK(ep == NULL, "a missing module without out-parameters is NULL too");

    return mbt_test_summary("TSTLOADX");
}

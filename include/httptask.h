/* HTTPTASK.H
** Check and clamp the operand of SET MAXTASK / SET MINTASK.
**
** Free of httpd.h so the pure decision logic unit-tests on the host as well as
** on MVS (see test/tsttask.c).  The name is 8 characters so it needs no asm()
** CSECT alias -- same reason as httpbody().
*/
#ifndef HTTPTASK_H
#define HTTPTASK_H

#define HTTP_TASK_OK        0   /* *value is set, 1..HTTP_TASK_MAX          */
#define HTTP_TASK_USAGE     1   /* no operand or a negative number; *value
                                   is not touched, the command changes
                                   nothing                                  */

#define HTTP_TASK_MAX       100

/* Read the operand of SET MAXTASK / SET MINTASK.  s may be NULL (the command
** had no operand).  Blanks before the number are skipped.  A number below 1
** is 1, a number above HTTP_TASK_MAX is HTTP_TASK_MAX, and a word that is not
** a number is 1.  A missing operand and a negative number are refused:
** strtoul() reads "-5" as a very large unsigned number, which would set 100.
*/
int httptask(const char *s, unsigned *value);

#endif /* HTTPTASK_H */

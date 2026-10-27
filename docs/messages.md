# Operator Messages

Every console message of HTTPD (httpd) is documented in the _HTTPD Reference_
(ML07-0002), appendix "Messages": the text, the severity, what it means and what
to do. The literals themselves live in
[`include/httpdmsg.h`](../include/httpdmsg.h), which is the source of record;
the appendix is its operator-facing form, and a change to one is a change to
the other.

| You want to know | Read |
|---|---|
| The meaning of one message and the action it asks for | _HTTPD Reference_ (ML07-0002), "Messages" |
| What is on the console, what is left to the Master Trace Table, and what the client gets instead | _HTTPD Reference_, "Messages", and _HTTPD User's Guide_ (ML07-0001), "Operating HTTPD" |
| A healthy start and stop, and a start that ends, with the messages in the order they appear | _HTTPD Reference_, "Messages"; _HTTPD User's Guide_, "Troubleshooting" |

The manuals are attached to each release from 4.5.0 on as
`ml07-0001-<edition>.pdf` and `ml07-0002-<edition>.pdf`, and their sources are
in [`docs/books`](books).

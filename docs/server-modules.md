# Writing Server Modules

How to write a server module for HTTPD (httpd) is described in the two manuals
that are kept next to the code in [`docs/books`](books). This page only says
where to look.

| You want to know | Read |
|---|---|
| A complete small module that builds, is deployed and answers a request | _HTTPD User's Guide_ (ML07-0001), "Writing a Server Module" |
| Routing a URL to a module, `MOD=`, `RESIDENT` and the module's authority | _HTTPD User's Guide_, "Routes and Server Modules"; _HTTPD Reference_ (ML07-0002), "Routes and Authentication Policy" |
| Every function of the `HTTPX` vector, the control blocks and the rules a module must keep | _HTTPD Reference_, "The Server Module Interface" |
| The display modules `/.dsrv`, `/.dm` and `/.dmtt` | _HTTPD Reference_, "The Display Modules" |

The manuals are attached to each release from 4.5.0 on as
`ml07-0001-<edition>.pdf` and `ml07-0002-<edition>.pdf`.

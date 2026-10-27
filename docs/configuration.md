# Configuring HTTPD

The configuration of HTTPD (httpd) is described in the two manuals that are
kept next to the code in [`docs/books`](books). This page only says where to
look.

| You want to know | Read |
|---|---|
| How the `HTTPPRM` member is chosen and read, the first configuration, routes, sessions and time limits as a task | _HTTPD User's Guide_ (ML07-0001), "Configuring HTTPD" and "Routes and Server Modules" |
| Every keyword of the member, its default, its range and what happens on a bad value | _HTTPD Reference_ (ML07-0002), "The HTTPPRM Keywords" |
| `MOD=` and `LOC=` routes, `AUTH=`, `RES=` and the resource check | _HTTPD Reference_, "Routes and Authentication Policy"; _HTTPD User's Guide_, "Authentication and Security" |
| The operator commands, including `F HTTPD,D CONFIG` | _HTTPD Reference_, "Console Commands"; _HTTPD User's Guide_, "Operating HTTPD" |
| The procedure `HTTPD`, its symbolic parameters, the sample library | _HTTPD Reference_, "The Started Task and the Package" |
| Serving files from the webroot | _HTTPD User's Guide_, "Serving Files" |

The manuals are attached to each release from 4.5.0 on as
`ml07-0001-<edition>.pdf` and `ml07-0002-<edition>.pdf`.

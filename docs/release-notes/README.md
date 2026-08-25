# Release notes

The published GitHub Release body for each tag, kept here as the source of
record. GitHub is not one, for two separate reasons.

**The workflow overwrites the body.** `make release` pushes a tag, which runs
mbt's shared `release.yml`. That job **deletes every existing release for the
tag** and recreates it with `gh release create --generate-notes` — a body built
from commit subjects. A hand-written body is not an input to that process and
does not survive a re-push of the same tag.

**A deleted release takes its body with it.** `v4.0.0` shipped an STC procedure
without the JES2 DDs its own documented CGI needed
([#256](https://github.com/mvslovers/httpd/issues/256)), so it was replaced
rather than amended. [`v4.0.0.md`](v4.0.0.md) is that body, captured before the
deletion — the 4.0.1 notes are built from it so the changelog survives the
correction.

## How a body here reaches a release

It does not, on its own. After the release workflow finishes:

```sh
gh release edit v4.0.1 --repo mvslovers/httpd \
    --notes-file docs/release-notes/v4.0.1.md
```

Order matters — run it after the workflow, or the recreate discards it.

## Writing the next one from the last

Reusing a body means substituting every version string in it, not just the
heading. `v4.0.0.md` carries `httpd-4.0.0-dist.zip` and five more artifact
names, the `HTTPD.V4R0M0.*` dataset qualifiers, and `V4R0M0` inside the sample
JCL it quotes. Diff the rendered file against the real `dist/` listing before
publishing.

One file per tag, named for it. Write it when the release is published, not
later.

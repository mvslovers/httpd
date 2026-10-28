# Builds the HTTPD manuals.  Needs typst and the bookmaster submodule
# (git submodule update --init docs/books/bookmaster).

TYPST ?= typst
BM    := bookmaster
# The PDF carries the edition: ml07-0001-0.pdf is the Draft of ML07-0001.
BOOKS := ml07-0001-0.pdf ml07-0002-0.pdf

.PHONY: all clean site

all: $(BOOKS)

ml07-0001-%.pdf: ml07-0001.typ $(wildcard guide/*.typ) $(wildcard ex/*/*) $(BM)/bookmaster.typ
	$(TYPST) compile --root . --font-path $(BM)/fonts --ignore-system-fonts $< $@

ml07-0002-%.pdf: ml07-0002.typ $(wildcard ref/*.typ) $(wildcard ex/*/*) $(BM)/bookmaster.typ
	$(TYPST) compile --root . --font-path $(BM)/fonts --ignore-system-fonts $< $@

# The web form, as Read the Docs builds it, into site/.
site:
	sh web/build.sh site

clean:
	rm -rf *.pdf site

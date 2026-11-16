# Minimal makefile for the NORI documentation.
#
#   make html       Sphinx manual    -> build/html
#   make latexpdf   Sphinx manual    -> build/latex/nori.pdf (needs LaTeX)
#   make doxygen    API reference    -> build/doxygen/html
#   make all        html + doxygen

SPHINXOPTS    ?= -W --keep-going
SPHINXBUILD   ?= sphinx-build
SOURCEDIR     = source
BUILDDIR      = build
DOXYGEN       ?= doxygen

.PHONY: help all html latexpdf doxygen clean

help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

all: html doxygen

html latexpdf:
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

doxygen:
	@mkdir -p $(BUILDDIR)/doxygen
	@$(DOXYGEN) doxygen.conf

clean:
	rm -rf $(BUILDDIR)

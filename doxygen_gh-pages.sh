#!/bin/sh
set -e

TMPDIR=$(mktemp -d)
git-new-workdir . $TMPDIR gh-pages

( \
	cat Doxyfile; \
	echo "OUTPUT_DIRECTORY=$TMPDIR"; \
	echo "HTML_OUTPUT=."; \
) | doxygen -

cd $TMPDIR
git add -A
git commit -m "update docs"
cd -
rm -r $TMPDIR

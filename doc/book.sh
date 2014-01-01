#!/bin/sh

OUT_DIR="`pwd`/out/"
XSL=book.xsl

rm -rf "$OUT_DIR"
mkdir "$OUT_DIR"
xsltproc --output "$OUT_DIR" "$XSL" book.xml

#phd -M -f xhtml -o "$OUT_DIR" -d book.xml


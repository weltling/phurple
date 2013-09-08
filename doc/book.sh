#!/bin/sh

OUT_DIR="`pwd`/out/"

mkdir "$OUT_DIR"
xsltproc --output "$OUT_DIR" book.xsl book.xml


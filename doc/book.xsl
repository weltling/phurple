<?xml version="1.0" encoding="utf-8"?> 
<!-- 
this uses phpdoc's styles etc. to create docs 
i'm using the following to generate the docs (while within the docs dir):

php /home/weltling/cvs/phpdoc/scripts/xml_proto.php purple ../purple.c
xsltproc book.xsl book.xml

be shure to set the rightly path to the phpdoc dir

-->
<xsl:stylesheet version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:import href="/home/weltling/cvs/phpdoc/phpbook/phpbook-xsl/html.xsl"/>
<xsl:param name="chunker.output.encoding" select="'utf-8'"/>
</xsl:stylesheet>

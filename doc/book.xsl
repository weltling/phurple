<?xml version="1.0" encoding="utf-8"?> 
<!-- 
this uses phpdoc's styles etc. to create docs 
i'm using the following to generate the docs (while within the docs dir):

php /home/weltling/cvs/phpdoc/scripts/xml_proto.php purple ../purple.c
xsltproc book.xsl book.xml

be shure to set the rightly path to the phpdoc dir

-->
<xsl:stylesheet version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:fo="http://www.w3.org/1999/XSL/Format">
	<xsl:import href="/usr/share/xml/docbook/stylesheet/nwalsh/xhtml/chunk.xsl"/>
	<xsl:param name="chunker.output.encoding" select="'utf-8'"/>
	<xsl:template match="//methodparam/parameter[@role='reference']">
		<!-- FIXME PHP call-by-ref args are shown as normal args, haven't figured
			 out yet how to do it right.  -->
		<!--<xsl:template match="parameter[@role='reference']">-->
		<!--<xsl:variable name="targets" select="key('role',@paramrole)"/>
		<xsl:variable name="target" select="$parameter[1]"/>-->
		<!--&amp;<xsl:value-of select="parameter[0]"/>-->
		<fo:inline color="blue">
			  		<xsl:call-template name="anchor"/>
					<xsl:apply-templates />
		  </fo:inline>
	</xsl:template>
</xsl:stylesheet>

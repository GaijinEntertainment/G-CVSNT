<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<xsl:import href="../xsl/htmlhelp/profile-htmlhelp.xsl" />
<xsl:import href="custom.xsl" />
<xsl:import href="custom_book_htmlhelp.xsl" />

<xsl:param name="htmlhelp.encoding">UTF-8</xsl:param>
<xsl:output method="html" encoding="UTF-8" indent="no" />

<xsl:param name="html.stylesheet">ede.css</xsl:param>

<!-- make all gui elements appear bold                                       -->
<!-- This adds html bold tags to each gui element (guibutton, guiicon,       -->
<!-- guilabel, guimenu, guimenuitem, guisubmenu)                             -->
<!-- applies to: html output only                                            -->
<!-- Comment the whole block out to make gui elements appear as regular      -->
<!-- text                                                                    -->

<xsl:template match="guibutton">
<b><xsl:call-template name="inline.charseq"/></b>
</xsl:template>

<xsl:template match="guiicon">
<b><xsl:call-template name="inline.charseq"/></b>
</xsl:template>

<xsl:template match="guilabel">
<b><xsl:call-template name="inline.charseq"/></b>
</xsl:template>

<xsl:template match="guimenu">
<b><xsl:call-template name="inline.charseq"/></b>
</xsl:template>

<xsl:template match="guimenuitem">
<b><xsl:call-template name="inline.charseq"/></b>
</xsl:template>

<xsl:template match="guisubmenu">
<b><xsl:call-template name="inline.charseq"/></b>
</xsl:template>

</xsl:stylesheet>
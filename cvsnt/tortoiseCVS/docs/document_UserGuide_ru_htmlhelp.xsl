<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<xsl:import href="../xsl/htmlhelp/profile-htmlhelp.xsl" />
<!-- <xsl:import href="e-novative.xsl" /> -->
<!-- <xsl:import href="e-novative_book.xsl" /> -->
<xsl:import href="custom.xsl" />
<xsl:import href="custom_book_htmlhelp.xsl" />

<xsl:output method="html" encoding="windows-1251" indent="no" />
<xsl:param name="htmlhelp.encoding">windows-1251</xsl:param>
<xsl:param name="l10n.gentext.default.language" select="'ru'"></xsl:param>

<xsl:param name="html.stylesheet">ede.css</xsl:param>

<!--<xsl:param name="chunker.output.encoding" select="'windows-1251'"/>-->
<!--<xsl:param name="saxon.character.representation" select="'native'"/>-->



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
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  version="1.0">
  
  <xsl:template match="email">
    <xsl:call-template name="inline.monoseq">
      <xsl:with-param name="content">
        <xsl:text>(</xsl:text>
        <xsl:call-template name="replaceCharsInString">
          <xsl:with-param name="stringIn" select="."/>
          <xsl:with-param name="charsIn" select="'@'"/>
          <xsl:with-param name="charsOut" select="' AT '"/>
        </xsl:call-template>
        <xsl:text>)</xsl:text>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>
  <xsl:template name="replaceCharsInString">
    <xsl:param name="stringIn"/>
    <xsl:param name="charsIn"/>
    <xsl:param name="charsOut"/>
    <xsl:choose>
      <xsl:when test="contains($stringIn,$charsIn)">
        <xsl:value-of select="concat(substring-before($stringIn,$charsIn),$charsOut)"/>
        <xsl:call-template name="replaceCharsInString">
          <xsl:with-param name="stringIn" select="substring-after($stringIn,$charsIn)"/>
          <xsl:with-param name="charsIn" select="$charsIn"/>
          <xsl:with-param name="charsOut" select="$charsOut"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$stringIn"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
  
  
  <xsl:template name="header.navigation">
    <xsl:param name="prev" select="/foo"/>
    <xsl:param name="next" select="/foo"/>
    <xsl:variable name="home" select="/*[1]"/>
    <xsl:variable name="up" select="parent::*"/>
    <xsl:if test="$suppress.navigation = '0'">
      <div style="background-image: url({$kde.common}top-middle.png); width: 100%; height: 131px;">
        <div style="position: absolute; 
                    right: 0px;">
          <img src="{$kde.common}top-right-konqueror.png"
            style="margin: 0px" alt="" />
          </div>
            <div style="position: absolute;
                        top: 25px; 
                        right: 100px; 
                        text-align: right; 
                        font-size: xx-large; 
                        font-weight: bold; 
                        text-shadow: #fff 0px 0px 5px; 
                        color: #444">
              <xsl:apply-templates select="." mode="title.markup"/>
            </div>
          </div>
          
          <div style="margin-top: 20px; background-color: #white; 
                      color: black;
                      margin-left: 20px; 
                      margin-right: 20px;">
            <div style="position: absolute; 
                        left: 20px;">
              <xsl:if test="count($prev)>0">
                <a accesskey="p">
                  <xsl:attribute name="href">
                    <xsl:call-template name="href.target">
                      <xsl:with-param name="object" select="$prev"/>
                    </xsl:call-template>
                  </xsl:attribute>
                  <xsl:call-template name="gentext.nav.prev"/>
                </a>
              </xsl:if>
            </div>
            <div style="position: absolute; 
                        right: 20px;">
              <xsl:if test="count($next)>0">
                <a accesskey="n">
                  <xsl:attribute name="href">
                    <xsl:call-template name="href.target">
                      <xsl:with-param name="object" select="$next"/>
                    </xsl:call-template>
                  </xsl:attribute>
                  <xsl:call-template name="gentext.nav.next"/>
                </a>
              </xsl:if>
            </div>
            <div class="navCenter">
              <xsl:choose>
                <xsl:when test="count($up) > 0 and $up != $home">
                  <xsl:apply-templates select="$up" mode="title.markup"/>
                </xsl:when>
                <xsl:otherwise>&#160;</xsl:otherwise>
              </xsl:choose>
            </div>
          </div>
          
        </xsl:if>
      </xsl:template>

<!-- ==================================================================== -->

<xsl:template name="footer.navigation">
  <xsl:param name="prev" select="/foo"/>
  <xsl:param name="next" select="/foo"/>
  <xsl:variable name="home" select="/*[1]"/>
  <xsl:variable name="up" select="parent::*"/>

  <xsl:if test="$suppress.navigation = '0'">

    <div style="background-color: #white; color: black; 
                margin-top: 20px; margin-left: 20px; 
                margin-right: 20px;">
      <div style="position: absolute; left: 20px;">
<xsl:if test="count($prev)>0">
              <a accesskey="p">
                <xsl:attribute name="href">
                  <xsl:call-template name="href.target">
                    <xsl:with-param name="object" select="$prev"/>
                  </xsl:call-template>
                </xsl:attribute>
                <xsl:call-template name="gentext.nav.prev"/>
              </a>
            </xsl:if>
          </div>
          <div style="position: absolute; right: 20px;">
            <xsl:if test="count($next)>0">
              <a accesskey="n">
                <xsl:attribute name="href">
                  <xsl:call-template name="href.target">
                    <xsl:with-param name="object" select="$next"/>
                  </xsl:call-template>
                </xsl:attribute>
                <xsl:call-template name="gentext.nav.next"/>
              </a>
            </xsl:if>
          </div>
          <div align="center">
            <xsl:choose>
              <xsl:when test="$home != .">
                <a accesskey="h">
                  <xsl:attribute name="href">
                    <xsl:call-template name="href.target">
                      <xsl:with-param name="object" select="$home"/>
                    </xsl:call-template>
                  </xsl:attribute>
                  <xsl:call-template name="gentext.nav.home"/>
                </a>
              </xsl:when>
              <xsl:otherwise>&#160;</xsl:otherwise>
            </xsl:choose>
          </div>
        </div>
    <div style="background-color: #white; 
	color: black;
        margin-left: 20px; 
	margin-right: 20px;">
      <div class="navLeft">
        <xsl:apply-templates select="$prev" mode="title.markup"/>
        <xsl:text>&#160;</xsl:text>
      </div>
      <div class="navRight">
        <xsl:text>&#160;</xsl:text>
        <xsl:apply-templates select="$next" mode="title.markup"/>
      </div>
      <div class="navCenter">
            <xsl:choose>
              <xsl:when test="count($up)>0">
                <a accesskey="u">
                  <xsl:attribute name="href">
                    <xsl:call-template name="href.target">
                      <xsl:with-param name="object" select="$up"/>
                    </xsl:call-template>
                  </xsl:attribute>
                  <xsl:call-template name="gentext.nav.up"/>
                </a>
              </xsl:when>
              <xsl:otherwise>&#160;</xsl:otherwise>
            </xsl:choose>
          </div>
          
          
          
      </div>
      <br/>
      <br/>

      <div class="bannerBottom" style="background-image: url({$kde.common}bottom-middle.png);
                                       background-repeat: x-repeat; 
                                       width: 100%; 
                                       height: 100px; 
                                       bottom:0px;">
	
        <div class="BannerBottomRight">
          <img src="{$kde.common}bottom-right.png" style="margin: 0px" alt=""/>
        </div>
        <div class="bannerBottomLeft">
          <img src="{$kde.common}bottom-left.png" style="margin: 0px;" alt=""/>
        </div>
<div id="comments" style="position:relative; top: 5px; left: 1em; height:85px; width: 50%; color: #cfe1f6">
	<p>Would you like to make a comment or contribute an update to this page?<br/>
        Send feedback to the <a href="mailto:petr@scribus.info" style="background:transparent; color:#cfe1f6; text-decoration: underline;">Sqliteman Author</a></p>
	</div>

      </div>

  </xsl:if>
</xsl:template>

</xsl:stylesheet>

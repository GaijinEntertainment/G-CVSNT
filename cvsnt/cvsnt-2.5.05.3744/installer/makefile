!Ifndef VERSION

client: cvsnt-client cvsnt-local-client trial-client

server: cvsnt-server 

all: cvsnt

release: cvsnt 

cvsnt: cvsnt-client cvsnt-server cvsnt-local-client

cvsnt-client:
	@for /f "usebackq delims=" %%a in (`cvs version -q`) do $(MAKE) -f makefile /nologo $(MAKEFLAGS) DOREGISTER=0 VERSION=%%a LICENSE=gpl.rtf $@

cvsnt-local-client:
	@for /f "usebackq delims=" %%a in (`cvs version -q`) do $(MAKE) -f makefile /nologo $(MAKEFLAGS) DOREGISTER=0 VERSION=%%a LICENSE=gpl.rtf $@

cvsnt-server:
	@for /f "usebackq delims=" %%a in (`cvs version -q`) do $(MAKE) -f makefile /nologo $(MAKEFLAGS) DOREGISTER=1 VERSION=%%a LICENSE=gpl.rtf $@

combined: suite

suite:	suite-server suite-client

suite-server:
	@for /f "usebackq delims=" %%a in (`cvs version -q`) do $(MAKE) -f makefile /nologo $(MAKEFLAGS) DOREGISTER=1 VERSION=%%a LICENSE=combined-license.rtf BINARY_TYPE=bin/Release $@

suite-client:
	@for /f "usebackq delims=" %%a in (`cvs version -q`) do $(MAKE) -f makefile /nologo $(MAKEFLAGS) DOREGISTER=1 VERSION=%%a LICENSE=combined-license.rtf BINARY_TYPE=bin/Release $@

trial:	trial-server trial-client

trial-server:
	@for /f "usebackq delims=" %%a in (`cvs version -q`) do $(MAKE) -f makefile /nologo $(MAKEFLAGS) DOREGISTER=1 VERSION=%%a LICENSE=combined-license.rtf BINARY_TYPE=bin/ReleaseTrial $@

trial-client:
	@for /f "usebackq delims=" %%a in (`cvs version -q`) do $(MAKE) -f makefile /nologo $(MAKEFLAGS) DOREGISTER=1 VERSION=%%a LICENSE=combined-license.rtf BINARY_TYPE=bin/ReleaseTrial $@

clean:
	del *.wixobj *.msi *.cab _license.rtf

cleanall:
	del *.wixobj *.msi *.cab _license.rtf

!Else
 
CVSNT_CLIENT_OBJS=cvsnt-client.wixobj ui.wixobj cvsnt.wixobj common.wixobj vc90rt.wixobj
CVSNT_SERVER_OBJS=cvsnt-server.wixobj ui.wixobj cvsnt.wixobj server.wixobj common.wixobj vc90rt.wixobj
CVSNT_LOCAL_CLIENT_OBJS=cvsnt-local-client.wixobj ui.wixobj cvsnt.wixobj local.wixobj common.wixobj vc90rt.wixobj
SUITE_CLIENT_OBJS=suite-client.wixobj ui.wixobj cvsnt.wixobj mfc71.wixobj mfc71u.wixobj  wincvs.wixobj mfc42u.wixobj msvcrt.wixobj winmerge.wixobj tortoisecvs.wixobj releasemanager.wixobj workspacemanager.wixobj ebook.wixobj scci.wixobj common.wixobj msvcr71.wixobj register.wixobj
SUITE_SERVER_OBJS=suite-server.wixobj ui.wixobj cvsnt.wixobj server.wixobj triggers.wixobj unison.wixobj wait4cvs.wixobj common.wixobj msvcr71.wixobj msvcrt.wixobj mfc71u.wixobj register.wixobj
TRIAL_CLIENT_OBJS=suite-client-trial.wixobj ui.wixobj cvsnt.wixobj wincvs.wixobj winmerge.wixobj tortoisecvs.wixobj workspacemanager.wixobj common.wixobj vc90rt.wixobj ebooktrial.wixobj 
TRIAL_SERVER_OBJS=suite-server-trial.wixobj ui.wixobj cvsnt.wixobj server.wixobj unison.wixobj wait4cvs.wixobj common.wixobj vc90rt.wixobj msvcrt.wixobj register.wixobj

CANDLEFLAGS=-dCVSNT_VERSION=$(VERSION) -dLICENSE=_license.rtf -dCVSNT_BASE=d:\cvsbin -dCVSNT64_BASE=d:\cvsbin64 -dWINCVS_BASE=d:\march-hare\wincvs -dWINMERGE_BASE=d:\march-hare\winmerge -dTORTOISECVS_BASE=d:\march-hare\TortoiseCVS -dMARCH_HARE=d:\march-hare\$(BINARY_TYPE) -dMERGEFILES="%ProgramFiles%/Common Files/Merge Modules" -dSYSFILES=d:/cvsdeps/2505/sysfiles -dDOREGISTER=$(DOREGISTER)
LIGHTFLAGS=-sw1076 -sw1055 -sw1056 -sice:ICE80

CVSNT_CLIENT_TARGET=cvsnt-client-$(VERSION).msi
CVSNT_LOCAL_CLIENT_TARGET=cvsnt-local-$(VERSION).msi
CVSNT_SERVER_TARGET=cvsnt-server-$(VERSION).msi
TRIAL_CLIENT_TARGET=suite-client-trial-$(VERSION).msi
TRIAL_SERVER_TARGET=suite-server-trial-$(VERSION).msi
SUITE_CLIENT_TARGET=suite-client-$(VERSION).msi
SUITE_SERVER_TARGET=suite-server-$(VERSION).msi

all: cvsnt

cvsnt: cvsnt-client cvsnt-server

cvsnt-client: $(CVSNT_CLIENT_TARGET)

cvsnt-local-client: $(CVSNT_LOCAL_CLIENT_TARGET)

cvsnt-server: $(CVSNT_SERVER_TARGET)

suite: suite-client suite-server

suite-client: $(SUITE_CLIENT_TARGET)

suite-server: $(SUITE_SERVER_TARGET)

trial: trial-client trial-server

trial-client: $(TRIAL_CLIENT_TARGET)

trial-server: $(TRIAL_SERVER_TARGET)

clean:
	del *.wixobj *.msi *.cab _license.rtf

$(CVSNT_CLIENT_TARGET): $(CVSNT_CLIENT_OBJS)
	copy $(LICENSE) _license.rtf
	light $(LIGHTFLAGS) -o $@ $(CVSNT_CLIENT_OBJS)
	signtool sign /n "March Hare Software Ltd" /d "March Hare CVSNT Client Installer" /du http://www.march-hare.com/cvsnt /t http://timestamp.verisign.com/scripts/timstamp.dll $@


$(CVSNT_LOCAL_CLIENT_TARGET): $(CVSNT_LOCAL_CLIENT_OBJS)
	copy $(LICENSE) _license.rtf
	light $(LIGHTFLAGS) -o $@ $(CVSNT_LOCAL_CLIENT_OBJS)
	signtool sign /n "March Hare Software Ltd" /d "March Hare CVSNT Local Client Installer" /du http://www.march-hare.com/cvsnt /t http://timestamp.verisign.com/scripts/timstamp.dll $@


$(CVSNT_SERVER_TARGET): $(CVSNT_SERVER_OBJS)
	copy $(LICENSE) _license.rtf
	light $(LIGHTFLAGS) -o $@ $(CVSNT_SERVER_OBJS)
	signtool sign /n "March Hare Software Ltd" /d "March Hare CVSNT Server Installer" /du http://www.march-hare.com/cvsnt /t http://timestamp.verisign.com/scripts/timstamp.dll $@

$(SUITE_CLIENT_TARGET): $(SUITE_CLIENT_OBJS)
	copy $(LICENSE) _license.rtf
	light $(LIGHTFLAGS) -o $@ $(SUITE_CLIENT_OBJS)
	signtool sign /n "March Hare Software Ltd" /d "CVS Suite Client Installer" /du http://www.march-hare.com/cvsnt /t http://timestamp.verisign.com/scripts/timstamp.dll $@

$(SUITE_SERVER_TARGET): $(SUITE_SERVER_OBJS)
	copy $(LICENSE) _license.rtf
	light $(LIGHTFLAGS) -o $@ $(SUITE_SERVER_OBJS)
	signtool sign /n "March Hare Software Ltd" /d "CVS Suite Server Installer" /du http://www.march-hare.com/cvsnt /t http://timestamp.verisign.com/scripts/timstamp.dll $@

$(TRIAL_CLIENT_TARGET): $(TRIAL_CLIENT_OBJS)
	copy $(LICENSE) _license.rtf
	light $(LIGHTFLAGS) -o $@ $(TRIAL_CLIENT_OBJS)
	signtool sign /n "March Hare Software Ltd" /d "March Hare CVSNT TortoiseCVS WinCVS Client Installer" /du http://www.march-hare.com/cvsnt /t http://timestamp.verisign.com/scripts/timstamp.dll $@

$(TRIAL_SERVER_TARGET): $(TRIAL_SERVER_OBJS)
	copy $(LICENSE) _license.rtf
	light $(LIGHTFLAGS) -o $@ $(TRIAL_SERVER_OBJS)
	signtool sign /n "March Hare Software Ltd" /d "March Hare CVSNT Server Installer with demo plugins" /du http://www.march-hare.com/cvsnt /t http://timestamp.verisign.com/scripts/timstamp.dll $@

.wxs.wixobj:
	candle $(CANDLEFLAGS) $<

.SUFFIXES: .wxs

!Endif

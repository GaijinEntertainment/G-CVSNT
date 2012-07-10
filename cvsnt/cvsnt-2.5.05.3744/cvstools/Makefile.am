## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/cvsapi $(includeopt) -DCVS_CONFIG_DIR=\"$(cvs_config_dir)\" -DCVS_LIBRARY_DIR=\"$(cvs_library_dir)\" -D_CVSTOOLS

libcvstools_la_SOURCES = \
	Cvsgui.cpp \
	Cvsgui.h \
	cvsroot.h \
	cvstools.h \
	export.h \
	EntriesParser.cpp \
	EntriesParser.h \
	GlobalSettings.h \
	plugin_interface.h \
	protocol_interface.h \
	ProtocolLibrary.cpp \
	ProtocolLibrary.h \
	RootSplitter.cpp \
	RootSplitter.h \
	Scramble.cpp \
	Scramble.h \
	ServerConnection.cpp \
	ServerConnection.h \
	ServerInfo.cpp \
	ServerInfo.h \
	trigger_interface.h \
	TriggerLibrary.cpp \
	TriggerLibrary.h \
	unix/GlobalSettings.cpp

libcvstools_la_LDFLAGS = -release $(VERSION) -L$(DESTDIR)/$(libdir)
libcvstools_la_LIBADD = ../cvsapi/libcvsapi.la ../cvsgui/libcvsgui.la

lib_LTLIBRARIES = libcvstools.la

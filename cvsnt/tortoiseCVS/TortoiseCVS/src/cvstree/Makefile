##########################################################################
# Variables

# Taget name
TARGET=cvstree

# Library file
BIN=libcvstree.a

# Source files
SOURCE=CvsLog.cpp CvsRepo.cpp

ifeq ($(DEBUG),1)
SOURCE+=lexer_d.cpp parser_d.cpp
else
SOURCE+=lexer.cpp parser.cpp
endif


# Compiler flags
CXXFLAGS=$(STLP_CFLAGS) $(WX_CFLAGS) -I . -Wall -fno-rtti -DWIN32 -D_MBCS -O2
ifeq ($(DEBUG),1)
CXXFLAGS+=-D_DEBUG 
else
CXXFLAGS+=-DNDEBUG 
endif

# TortoiseCVS base dir
TORTOISE_SRC=..

# Include Make environment
include ../Makeenv

# Include Make program
include ../Makeprog


lexer.cpp : lexer.ll
	mkdir -p $(@D)
	$(FLEX) -+ -d -Sflex.skl -o$@ $<

lexer_d.cpp : lexer.ll
	mkdir -p $(@D)
	$(FLEX) -+ -d -Sflex.skl -o$@ $<

parser.cpp : parser.yy
	$(SED) -e "s/\r//" < $(BISON_PKGDATADIR)/$(BISON_SKEL) > $(BISON_PKGDATADIR)/$(BISON_SKEL_TMP) ; \
	export BISON_PKGDATADIR=$(BISON_PKGDATADIR) ; \
	export M4=$(M4) ; \
	$(BISON) --name-prefix=yycvs -o parser.c -d $< -S $(BISON_SKEL_TMP) 2>&1 ; \
	rm  $(BISON_PKGDATADIR)/$(BISON_SKEL_TMP) ; \
	$(SED) -e "s/parser\.c/parser\.cpp/g" <  parser.c > $@ ; \
	rm -f parser.c

parser_d.cpp : parser.yy
	$(SED) -e "s/\r//" < $(BISON_PKGDATADIR)/$(BISON_SKEL) > $(BISON_PKGDATADIR)/$(BISON_SKEL_TMP) ; \
	export BISON_PKGDATADIR=$(BISON_PKGDATADIR) ; \
	export M4=$(M4) ; \
	$(BISON) --name-prefix=yycvs -o parser.c -d $< -S $(BISON_SKEL_TMP) 2>&1 ; \
	rm  $(BISON_PKGDATADIR)/$(BISON_SKEL_TMP) ; \
	$(SED) -e "s/parser\.c/parser_d\.cpp/g" <  parser.c > $@ ; \
	rm -f parser.c

clean ::
	-rm -f lexer.cpp parser.cpp lexer_d.cpp parser_d.cpp parser.h

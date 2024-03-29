# Makefile automatically generated, do not edit!
# This output (only this Makefile) is Public Domain.
#
#@MD5TINOIGN@ Creation date: Sun May  7 22:18:16 CEST 2023
#
# This file is based on following files:
#@MD5TINOIGN@ 1: Makefile.tino
#@MD5TINOIGN@ 2: tino/Makefile.proto

#
#@MD5TINOIGN@ included: Makefile.tino
#

      PROGS=ptybuffer ptybufferconnect
       OBJS=

 INSTALLBIN=
INSTALLSBIN=
 INSTALLLIB=
 INSTALLETC=
 INSTALLMAN=
INSTALLSMAN=
 INSTALLINF=
INSTALLSINF=

 ADD_CFLAGS=
ADD_CCFLAGS=
ADD_LDFLAGS=
 ADD_LDLIBS=-lutil
      CLEAN=
  CLEANDIRS=
  DISTCLEAN=php/sock/shell
   TINOCOPY=

INSTALLPATH=/usr/local

#
#@MD5TINOIGN@ included: tino/Makefile.proto
#

# Automatically generated from "PROGS" above
      PROG1=ptybuffer
      PROG2=ptybufferconnect

# Override those in Makefile.tino if needed:
 STD_CFLAGS=-g -Wall -Wno-unused-function -O3 -Wno-error=unused-value -Wno-error=unused-function
 STD_CCFLAGS=-g -Wall -Wno-unused-function -O3 -Wno-error=unused-value -Wno-error=unused-function
STD_LDFLAGS=
 STD_LDLIBS=
    BINPATH=bin
   SBINPATH=sbin
    LIBPATH=lib
    ETCPATH=etc
    MANPATH=man
   SMANPATH=share/man
    INFPATH=info
   SINFPATH=share/info

# Except for the compiler generated dependencies at the end
# from here no changes shall be needed.

     CFLAGS=$(DBG_FLAGS) $(DBG_CFLAGS) $(STD_CFLAGS) $(ADD_CFLAGS) -I"$(HERE)"
   CXXFLAGS=$(DBG_CCFLAGS) $(STD_CCFLAGS) $(ADD_CCFLAGS) -I"$(HERE)"
    LDFLAGS=$(DBG_LDFLAGS) $(STD_LDFLAGS) $(ADD_LDFLAGS)
     LDLIBS=$(DBG_LDLIBS) $(STD_LDLIBS) $(ADD_LDLIBS)

VERSIONFILE=$(PROG1)_version
VERSIONNAME=$(VERSIONFILE)
 VERSIONEXT=h
     COMMON=			\
		$(VERSIONFILE).$(VERSIONEXT)	\
		$(TINOINC)	\
		$(TINOLIB)	\

       GAWK=awk
      TOUCH=touch
      MKDIR=mkdir

         CP=cp
      STRIP=strip

       HERE=$(PWD)

# Semi-automatically generated for CygWin (*.exe)

  PROGS_EXE=			\
		$(PROG1).exe	\
		$(PROG2).exe	\

.PHONY: all static install it clean distclean dist tar diff always

# Targets considered to work for all systems with GNU MAKE and GNU AWK

all::	$(SUBDIRS) $(PROGS)

test::	all Tests
	$(PWD)/tino/Makefile-tests.sh Tests

# To use this you need to do:
#	ln -s tinolib/diet .
#	make static
# This is experimental.

static::
	[ -f diet.distignore~ ] || $(MAKE) clean
	$(TOUCH) diet.distignore~
	[ ! -d diet ] || $(MAKE) -C diet diet
	[ ! -d diet ] || $(MAKE) CC="$(PWD)/diet/tinodiet.sh --tinodiet"
	[ -d diet ] || $(MAKE) CC="$(PWD)/tino/Makefile-diet.sh --tinodiet"

Makefile:	Makefile.md5
	$(TOUCH) Makefile

Makefile.md5:	$(VERSIONFILE).$(VERSIONEXT) always
	@[ -z '$(HERE)' ] || $(GAWK) -vHERE="$(HERE)" -vMAKE="$(MAKE)" -vTINOCOPY="$(TINOCOPY)" 'BEGIN { \
	if ((getline < "tino/Makefile")>0 && \
	    (getline < "tino/Makefile.proto")>0 && \
	    (getline < "tino/Makefile.awk")>-1) \
		system(MAKE " -C tino tino HERE='\''" HERE "'\'' TINOCOPY='\''" TINOCOPY "'\''"); \
	else if ((getline < "Makefile.md5")<0)	 \
		printf "" >"Makefile.md5"; \
	}'

$(VERSIONFILE).h:	VERSION
	$(GAWK) -vPROG="$(VERSIONNAME)" '{ gsub(/-/,"_",PROG); print "#define " toupper(PROG) " \"" $$0 "\"" }' VERSION > "$(VERSIONFILE).h"

$(VERSIONFILE).py:	VERSION
	$(GAWK) -vPROG="$(VERSIONNAME)" '{ gsub(/-/,"_",PROG); print "#"; print ""; print toupper(PROG) "=\"" $$0 "\"" }' VERSION > "$(VERSIONFILE).py"

# Poor man's install
# Generated from PROGS and INSTALL* above

install::
	$(RM) "$(INSTALLPATH)/bin/$(PROG1)"
	$(MKDIR) -pm755 "$(INSTALLPATH)/bin"
	$(CP) "$(PROG1)" "$(INSTALLPATH)/bin/$(PROG1)"
	$(STRIP) "$(INSTALLPATH)/bin/$(PROG1)"
	$(RM) "$(INSTALLPATH)/bin/$(PROG2)"
	$(MKDIR) -pm755 "$(INSTALLPATH)/bin"
	$(CP) "$(PROG2)" "$(INSTALLPATH)/bin/$(PROG2)"
	$(STRIP) "$(INSTALLPATH)/bin/$(PROG2)"

# Special targets considered to work for all unix like systems
# like CygWin

it::	all
	[ ".$(PWD)" != ".$(HERE)" ] || [ -f VERSION ] || \
	{ UP="`dirname "$(HERE)"`"; $(MAKE) -C"$$UP" it HERE="$$UP"; }

clean::
	$(RM) *.o *.d *~ .*~ */*~ $(CLEAN)
	-$(MAKE) -C tino clean HERE="$(HERE)"

distclean::	clean
	$(RM) "$(VERSIONFILE).h" $(PROGS) $(PROGS_EXE) $(DISTCLEAN)
	$(RM) core core.* .#*
	-$(MAKE) -C tino distclean HERE="$(HERE)"

# Special targets in presence of tinolib
# (subdirectory tino)

dist::	distclean
	-$(MAKE) -C tino dist HERE="$(HERE)" DEBUGS="$(DBG_CFLAGS)$(DBG_LDFLAGS)$(DBG_LDLIBS)"

tar::	distclean
	-$(MAKE) -C tino tar HERE="$(HERE)"

diff::
	-$(MAKE) -C tino diff HERE="$(HERE)"

# Automatically generated from $(SUBDIRS):

# automatically generated dependencies
$(PROG1).o:	$(COMMON)
$(PROG1):	$(PROG1).o $(OBJS) $(LIBS)
$(PROG2).o:	$(COMMON)
$(PROG2):	$(PROG2).o $(OBJS) $(LIBS)

# compiler generated dependencies, remove if incorrect

# included: ptybuffer.d
$(PROG1).o:  ptybuffer.c tino/file.h tino/sysfix.h tino/sysfix_cygwin.h \
 tino/sysfix_diet.h tino/sysfix_linux.h tino/sysfix_osx.h tino/type.h \
 tino/debug.h tino/fatal.h tino/ex.h tino/arg.h tino/alloc.h tino/debug.h \
 tino/err.h tino/fatal.h tino/sock_select.h tino/sock.h tino/file.h \
 tino/alloc.h tino/threads.h tino/strprintf.h tino/str.h tino/syscap.h \
 tino/sock_select_old.h tino/slist.h tino/getopt.h tino/proc.h \
 tino/filetool.h tino/sock_tool.h tino/signals.h tino/assoc.h tino/tmp.h \
 ptybuffer_version.h


# included: ptybufferconnect.d
$(PROG2).o:  ptybufferconnect.c tino/file.h tino/sysfix.h \
 tino/sysfix_cygwin.h tino/sysfix_diet.h tino/sysfix_linux.h \
 tino/sysfix_osx.h tino/type.h tino/sockbuf.h tino/sock_select.h \
 tino/sock.h tino/file.h tino/fatal.h tino/ex.h tino/arg.h tino/alloc.h \
 tino/debug.h tino/err.h tino/threads.h tino/strprintf.h tino/str.h \
 tino/syscap.h tino/sock_select_old.h tino/buf.h tino/codec.h \
 tino/getopt.h tino/codec.h ptybuffer_version.h

# end

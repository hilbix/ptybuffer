# $Header$
#
# $Log$
# Revision 1.2  2004-05-21 10:40:03  tino
# -lefence
#
# Revision 1.1  2004/05/19 20:23:43  tino
# first commit
#

       PROG=ptybuffer
      PROGS=$(PROG)
       OBJS=
     CFLAGS=-g -Wall -O3
    LDFLAGS=-lutil #-lefence
VERSIONFILE=$(PROG)_version
VERSIONNAME=$(PROG)_VERSION
     COMMON=Makefile tino_common.h tino/lib.h $(VERSIONFILE).h

         CP=cp
      STRIP=strip

all:	$(PROGS)
	$(MAKE) -C tino all

tino/lib.h:
	$(MAKE) -C tino lib.h

clean:
	$(MAKE) -C tino clean
	$(RM) $(PROGS) *.o *~ sock sock.tmp

distclean:	clean
	$(MAKE) -C tino distclean
	$(RM) $(VERSIONFILE).h core core.*

dist:	distclean
	$(MAKE) -C tino dist

install:
	for a in $(PROGS); do $(RM) "$$HOME/bin/$$a"; \
	$(CP) "$$a" "$$HOME/bin/$$a"; $(STRIP) "$$HOME/bin/$$a"; done

$(VERSIONFILE).h:	VERSION Makefile
	echo "#define $(VERSIONNAME) \"`cat VERSION`\"" > $(VERSIONFILE).h

$(PROG):	$(PROG).o $(OBJS)
$(PROG).o:	$(PROG).c $(COMMON)

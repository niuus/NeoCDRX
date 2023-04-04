##########################
# Makefile for Z80 Core  #
##########################

BUILDDIR=../../build_cpu

#setup C Flags
CFLAGS= -I. -I./$(BUILDDIR) -Wall -O3 -fomit-frame-pointer -fno-ident -DINLINE="static inline" -DCLEANBUILD=1 \
		-fstrict-aliasing -D__ASMPREFIX__

all: $(BUILDDIR)/libz80.a

clean:
	rm -f $(BUILDDIR)/libz80.a

$(BUILDDIR)/libz80.a: $(BUILDDIR)/z80.o $(BUILDDIR)/z80daisy.o
	@$(AR) -r $@ $^

$(BUILDDIR)/z80.o: z80.c z80.h
	@echo ""
	@echo "*** Mame Z80 Core 3.6 ******************************************************"
	@echo "    Copyright (C) 1998,1999,2000 Juergen Buchmueller, all rights reserved."
	@echo "****************************************************************************"
	@echo ""
	@$(CC) $(CFLAGS) -c z80.c -o $(BUILDDIR)/z80.o

$(BUILDDIR)/z80daisy.o: z80daisy.c z80daisy.h
	@$(CC) $(CFLAGS) -c z80daisy.c -o $(BUILDDIR)/z80daisy.o

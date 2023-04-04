##########################
# Makefile for 68k Core  #
##########################

CCE = @gcc
BUILDDIR=../../build_cpu

#setup C Flags
CFLAGS= -I. -I./$(BUILDDIR) -O3 -fomit-frame-pointer -fno-ident -funsigned-char 

# 68k instructions file
INFILE = neocd68k.c

all: $(BUILDDIR)/libmc68000.a

clean: rm -f $(BUILDDIR)/libmc68000.a

$(BUILDDIR)/libmc68000.a: $(BUILDDIR)/m68kcpu.o $(BUILDDIR)/m68kops.o $(BUILDDIR)/m68kopac.o $(BUILDDIR)/m68kopdm.o $(BUILDDIR)/m68kopnz.o
	@$(AR) -r $@ $^

$(BUILDDIR)/m68kcpu.o: $(BUILDDIR)/m68kops.h m68k.h m68kconf.h
	@$(CC) $(CFLAGS) -c m68kcpu.c -o $(BUILDDIR)/m68kcpu.o

$(BUILDDIR)/m68kops.o: $(BUILDDIR)/m68kmake.exe $(BUILDDIR)/m68kops.h $(BUILDDIR)/m68kops.c m68k.h m68kconf.h
	@$(CC) $(CFLAGS) -c $(BUILDDIR)/m68kops.c -o $(BUILDDIR)/m68kops.o

$(BUILDDIR)/m68kopac.o: $(BUILDDIR)/m68kmake.exe $(BUILDDIR)/m68kops.h $(BUILDDIR)/m68kopac.c m68k.h m68kconf.h
	@$(CC) $(CFLAGS) -c $(BUILDDIR)/m68kopac.c -o $(BUILDDIR)/m68kopac.o

$(BUILDDIR)/m68kopdm.o: $(BUILDDIR)/m68kmake.exe $(BUILDDIR)/m68kops.h $(BUILDDIR)/m68kopdm.c m68k.h m68kconf.h
	@$(CC) $(CFLAGS) -c $(BUILDDIR)/m68kopdm.c -o $(BUILDDIR)/m68kopdm.o

$(BUILDDIR)/m68kopnz.o: $(BUILDDIR)/m68kmake.exe $(BUILDDIR)/m68kops.h $(BUILDDIR)/m68kopnz.c m68k.h m68kconf.h
	@$(CC) $(CFLAGS) -c $(BUILDDIR)/m68kopnz.c -o $(BUILDDIR)/m68kopnz.o

$(BUILDDIR)/m68kops.h: $(BUILDDIR)/m68kmake.exe
	@echo ""
	@echo "*** Musashi 3.3 ************************************************************"
	@$(BUILDDIR)/m68kmake.exe $(BUILDDIR) $(INFILE)
	@echo "****************************************************************************"
	@echo ""

$(BUILDDIR)/m68kmake.exe: m68kmake.c $(INFILE)
	$(CCE) $(WARNINGS) m68kmake.c -o $(BUILDDIR)/m68kmake.exe

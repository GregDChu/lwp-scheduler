CC 	= gcc

CFLAGS  = -Wall -g -I .

LD 	= gcc

LDFLAGS  = -Wall -g -L/home/pn-cs453/Given/Asgn2


PUBFILES =  README  hungrymain.c  libPLN.a  libsnakes.a  lwp.h\
	    numbersmain.c  snakemain.c  snakes.h

#TARGET =  pn-cs453@hornet:Given/asgn2

PROGS	= snakes nums hungry test

LIBS = liblwp.so

SNAKEOBJS  = snakemain.o 

HUNGRYOBJS = hungrymain.o 

NUMOBJS    = numbersmain.o

TESTOBJS = test.o

MAGICOBJS = magic64.o

LWPOBJS = lwp.o

OBJS	= $(SNAKEOBJS) $(HUNGRYOBJS) $(NUMOBJS) $(MAGICOBJS) $(TESTOBJS) $(LWPOBJS)

SRCS	= snakemain.c numbersmain.c

HDRS	= 

EXTRACLEAN = core $(PROGS) $(LIBS)

all: 	$(PROGS) liblwp.so

clean: 
	rm -f $(OBJS) *~ TAGS
	@rm -f $(EXTRACLEAN)

snakes: snakemain.o libsnakes.a $(LIBS)
	$(LD) $(LDFLAGS) -o snakes snakemain.o -L. -lncurses -lsnakes -lPLN

hungry: hungrymain.o libsnakes.a $(LIBS)
	$(LD) $(LDFLAGS) -o hungry hungrymain.o -L. -lncurses -lsnakes -lPLN

nums: numbersmain.o $(LIBS)
	$(LD) $(LDFLAGS) -o nums numbersmain.o -L. -lPLN

test: test.c $(LIBS)
	$(LD) $(CFLAGS) -c test.c -o test.o
	$(LD) $(CFLAGS) -o test test.o -L. -llwp

hungrymain.o: lwp.h snakes.h
#	$(LD) -o hungrymain.o -c hungrymain.c -L. -lPLN -lsnakes -lncurses

snakemain.o: lwp.h snakes.h
#	$(LD) -o snakemain.o -c snakemain.c -L. -lPLN -lsnakes -lncurses

numbermain.o: lwp.h
#	$(LD) -o numbersmain.o -c numbersmain.c -L. -lPLN -lsnakes -lncurses

magic64.o: magic64.S
	$(LD) -o magic64.o -c magic64.S

#libPLN.a: lwp.c magic64.o
#	gcc -g -c lwp.c
#	ar r libPLN.a lwp.o magic64.o
#	rm lwp.o

liblwp.so: lwp.c magic64.o
	$(LD) $(CFLAGS) -fPIC -c lwp.c -o lwp.o
	$(LD) $(CFLAGS) -fPIC -shared -o liblwp.so lwp.o magic64.o

pub:
	scp $(PUBFILES) $(TARGET)


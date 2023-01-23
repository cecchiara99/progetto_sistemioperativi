CC		=  gcc
AR              =  ar
CFLAGS	        += -std=c99 -Wall -Werror -g
ARFLAGS         =  rvs
INCDIR          = ./includes 
INCLUDES	= -I. -I $(INCDIR) 
LDFLAGS 	= -L. 
LIBS            = -lpthread

TARGETS		=	farm	\
				generafile

.PHONY: all clean cleanall
.SUFFIXES: .c .h

%: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

all		: $(TARGETS)

farm: farm.o libQueue.a threadLib.a mwLib.a cLib.a  libBqueue.a
	$(CC) $(CCFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $^ $(LIBS)

cLib.a: ./collector/collector.o ./queue/queue.o
	$(AR) $(ARFLAGS) $@ $^
mwLib.a: ./master/masterthread.o ./queue/queue.o ./threadpool/threadpool.o ./queue/queue.o ./boundedqueue/boundedqueue.o
	$(AR) $(ARFLAGS) $@ $^
libQueue.a: ./queue/queue.o ./queue/queue.h
	$(AR) $(ARFLAGS) $@ $<

libBqueue.a: ./boundedqueue/boundedqueue.o ./boundedqueue/boundedqueue.h
	$(AR) $(ARFLAGS) $@ $<

threadLib.a: ./threadpool/threadpool.o ./threadpool/threadpool.h
	$(AR) $(ARFLAGS) $@ $< 

clean		: 
	rm -f $(TARGETS) 

cleanall	: clean
	rm -f *.o *~ *.a ./queue/*.o ./threadpool/*.o ./master/*.o ./collector/*.o ./threadpool/*~ ./queue/*~ master/*~ collector/*~ ./boundedqueue/*~ ./boundedqueue/*.o  ./expected.txt ./generafile ./testdir/*.dat ./testdir/testdir2/*.dat ./*.dat
	rm -d ./testdir/testdir2/ ./testdir/

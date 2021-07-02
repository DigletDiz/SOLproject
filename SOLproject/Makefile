CC		=  gcc
AR              =  ar
CFLAGS	        += -std=c99 -Wall -Werror -g
ARFLAGS         =  rvs
INCDIR          = ./utils/includes -I ./threadpool
INCLUDES	= -I. -I $(INCDIR)
LDFLAGS 	= -L.
OPTFLAGS	= #-O3 
LIBS            = -lpthread

TARGETS		= server client

.PHONY: all clean cleanall
.SUFFIXES: .c .h

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all		: $(TARGETS)

server: server.o worker.o libPool.a libDataStruct.a
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

client: client.o libClient.a libDataStruct.a
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $^

libPool.a: ./threadpool/threadpool.o ./threadpool/threadpool.h
	$(AR) $(ARFLAGS) $@ $^

libDataStruct.a: icl_hash.o ./utils/includes/icl_hash.h list.o ./utils/includes/list.h queue.o ./utils/includes/queue.h
	$(AR) $(ARFLAGS) $@ $^

libClient.a: api.o
	$(AR) $(ARFLAGS) $@ $^

server.o: server.c

client.o: client.c

worker.o: worker.c

./threadpool/threadpool.o: ./threadpool/threadpool.c 

api.o: api.c

icl_hash.o: icl_hash.c

list.o: list.c

queue.o: queue.c

clean		: 
	rm -f $(TARGETS)
cleanall	: clean
	\rm -f *.o *~ *.a ./cs_sock threadpool/*.o threadpool/*~
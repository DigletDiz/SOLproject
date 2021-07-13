CC		=  gcc
AR              =  ar
CFLAGS	        += -std=c99 -Wall -Werror -g
ARFLAGS         =  rvs
INCDIR          = ./includes
INCLUDES	= -I. -I $(INCDIR)
LDFLAGS 	= -L.
OPTFLAGS	= #-O3 
LIBS            = -lpthread

TARGETS		= server client

.PHONY: all clean cleanall
.SUFFIXES: .c .h

%.o: ./src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all		: $(TARGETS)

server: server.o worker.o libPool.a libDataStruct.a
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

client: client.o libClient.a libDataStruct.a
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $^

libPool.a: threadpool.o ./includes/threadpool.h
	$(AR) $(ARFLAGS) $@ $^

libDataStruct.a: icl_hash.o ./includes/icl_hash.h list.o ./includes/list.h queue.o ./includes/queue.h
	$(AR) $(ARFLAGS) $@ $^

libClient.a: api.o
	$(AR) $(ARFLAGS) $@ $^

server.o: ./src/server.c

client.o: ./src/client.c

worker.o: ./src/worker.c

threadpool.o: ./src/threadpool.c 

api.o: ./src/api.c

icl_hash.o: ./src/icl_hash.c

list.o: ./src/list.c

queue.o: ./src/queue.c

clean		: 
	rm -f $(TARGETS)
cleanall	: clean
	\rm -f *.o *~ *.a ./cs_sock threadpool/*.o threadpool/*~

test1: $(TARGETS)
	./scripts/test1.sh

test2: $(TARGETS)
	./scripts/test2.sh
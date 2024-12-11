CC=gcc
CFLAGS+=-ggdb -D_GNU_SOURCE -DMLS_DEBUG -O0 -Wall

all: use-after-free

use-after-free: mls.o

clean:
	-${RM} *.o *~ use-after-free

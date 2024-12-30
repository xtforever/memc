TARGET=adv-string
CC=gcc
CFLAGS+=-ggdb -D_GNU_SOURCE -DMLS_DEBUG -O0 -Wall 

all: ${TARGET}

${TARGET}: mls.o m_tool.o

clean:
	-${RM} *.o *~ ${TARGET}

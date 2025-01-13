debug_enable ?= 1
TARGET=hashmap
CC=gcc
CFLAGS_DEBUG=-ggdb -D_GNU_SOURCE -DMLS_DEBUG -O0 -Wall
CFLAGS_OPT=-D_GNU_SOURCE -O3 -Wall -fdata-sections -ffunction-sections -Wl,--gc-sections

ifeq ($(debug_enable),1)
COMPTAG="\"DEBUG_$(shell date)\""
CFLAGS=$(CFLAGS_DEBUG)
OBJ=.d
else
COMPTAG="\"PROD_$(shell date)\"" 
CFLAGS=$(CFLAGS_OPT)
OBJ=.
endif
CFLAGS+= -DCOMPTAG=$(COMPTAG)

all: ${TARGET}${OBJ} mmsql${OBJ}

%.do: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.d: %.do
	$(CC) $(CFLAGS) -o $@ $^

${TARGET}${OBJ}: mls${OBJ}o m_tool${OBJ}o ${TARGET}${OBJ}o

mmsql${OBJ}: mls${OBJ}o m_tool${OBJ}o mmsql${OBJ}o m_mysql${OBJ}o -lmysqlclient

clean:
	-${RM} *.o *.do *~ ${TARGET} ${TARGET}.d mmsql.d


debug_enable ?= 1
TARGET=dash 
CC=gcc
CFLAGS_DEBUG=-ggdb -D_GNU_SOURCE -DMLS_DEBUG -O0 -Wall -fstack-protector-all -fsanitize=address -fms-extensions
CFLAGS_OPT=-D_GNU_SOURCE -Ofast -Wall -fdata-sections -ffunction-sections -Wl,--gc-sections -fms-extensions

ifeq ($(debug_enable),1)
COMPTAG="\"DEBUG_$(shell date)\""
CFLAGS=$(CFLAGS_DEBUG)
OBJ=d
else
COMPTAG="\"PROD_$(shell date)\"" 
CFLAGS=$(CFLAGS_OPT)
OBJ=
endif
CFLAGS+= -DCOMPTAG=$(COMPTAG)

DEPS=mls.o${OBJ} m_tool.o${OBJ} var5.o${OBJ} ctx.o${OBJ}
NEW_TARGETS := $(patsubst %,%.exe${OBJ},$(TARGET))
LDLIBS=-lmysqlclient

all: ${NEW_TARGETS}



%.od: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.exe: %.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.exed: %.od 
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(NEW_TARGETS): $(DEPS)

clean:
	-${RM} *.o *.od *~ *.exe *.exed

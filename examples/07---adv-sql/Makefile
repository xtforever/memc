debug_enable ?= 1
TARGET=dash
CC=gcc
CFLAGS_DEBUG=-ggdb -D_GNU_SOURCE -DMLS_DEBUG -O0 -Wall -fstack-protector-all -fsanitize=address -fms-extensions
CFLAGS_OPT=-D_GNU_SOURCE -Ofast -Wall -fdata-sections -ffunction-sections -Wl,--gc-sections
LDFLAGS+=-lmysqlclient

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

all: ${TARGET}.exe${OBJ} 

%.do: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.exe: %.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.exed: %.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

${TARGET}.exe${OBJ}: mls.${OBJ}o m_tool.${OBJ}o m_mysql.${OBJ}o var5.${OBJ}o ctx.${OBJ}o 

clean:
	-${RM} *.o *.do *~ *.exe *.exed


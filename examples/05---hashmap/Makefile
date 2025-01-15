debug_enable ?= 1
TARGET=hashmap
CC=gcc
CFLAGS_DEBUG=-ggdb -D_GNU_SOURCE -DMLS_DEBUG -O0 -Wall
CFLAGS_OPT=-D_GNU_SOURCE -Ofast -Wall -fdata-sections -ffunction-sections -Wl,--gc-sections

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

all: ${TARGET}.exe${OBJ} mmsql.exe${OBJ} cmk-inventory.exe${OBJ}

%.do: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.exe: %.o
	$(CC) $(CFLAGS) -o $@ $^

%.exed: %.o
	$(CC) $(CFLAGS) -o $@ $^

${TARGET}.exe${OBJ}: mls.${OBJ}o m_tool.${OBJ}o m_csv.${OBJ}o

mmsql.exe${OBJ}: mls.${OBJ}o m_tool.${OBJ}o m_mysql.${OBJ}o m_csv.${OBJ}o  -lmysqlclient

cmk-inventory.exe${OBJ}: mls.${OBJ}o m_tool.${OBJ}o m_mysql.${OBJ}o m_csv.${OBJ}o  -lmysqlclient

clean:
	-${RM} *.o *.do *~ ${TARGET} ${TARGET}.d mmsql.d


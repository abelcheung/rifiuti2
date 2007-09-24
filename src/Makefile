CC = gcc
CFLAGS = $(shell pkg-config --cflags glib-2.0) -Wall -ggdb
LDFLAGS = $(shell pkg-config --libs glib-2.0) -lm

all: rifiuti rifiuti-vista

SRC_RIFIUTI = rifiuti.c utils.c
OBJ_RIFIUTI = $(SRC_RIFIUTI:.c=.o)

SRC_VISTA = rifiuti-vista.c utils.c
OBJ_VISTA = $(SRC_VISTA:.c=.o)

rifiuti: $(OBJ_RIFIUTI)
	$(CC) -o $@ $(OBJ_RIFIUTI) $(LDFLAGS)

rifiuti-vista: $(OBJ_VISTA)
	$(CC) -o $@ $(OBJ_VISTA) $(LDFLAGS)

clean:
	rm -f rifiuti rifiuti.exe rifiuti-vista rifiuti-vista.exe $(OBJ_RIFIUTI) $(OBJ_VISTA)

.c.o:
	$(CC) -c -o $@ $< $(CFLAGS)

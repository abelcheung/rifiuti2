CFLAGS = $(shell pkg-config --cflags glib-2.0) -Wall -ggdb
LDFLAGS = $(shell pkg-config --libs glib-2.0) -lm

all: rifiuti

rifiuti: rifiuti.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o rifiuti $<

clean: 
	rm -f rifiuti

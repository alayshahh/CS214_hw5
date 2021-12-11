OUTPUT = csapp queue
CFLAGS = -g -Wall -Wvla
LFLAGS = -L lib -lSDL2 -lSDL2_image -lSDL2_ttf

%.o: %.c %.h
	gcc $(CFLAGS) -c -o $@ $<

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

all: $(OUTPUT)

runclient: $(OUTPUT)
	LD_LIBRARY_PATH=lib ./client


csapp: csapp.o 
	gcc $(CFLAGS) -c csapp.c -lpthread

echoclient: echoclient.c
	gcc $(CFLAGS) echoclient.c csapp.o -lpthread -o $@

queue:
	gcc $(CFLAGS) -c queue.c 	


clean:
	rm -f  *.o

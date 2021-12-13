OUTPUT = csapp queue ./server/server ./client/client
CFLAGS = -g -Wall -Wvla
INCFLAGS = -I inc -D_REENTRANT
LFLAGS = -L lib -lSDL2 -lSDL2_image -lSDL2_ttf
CLIENT_DIR := ./client
SERVER_DIR := ./server
CURRENT_DIR := .



%.o: %.c %.h
	gcc $(CFLAGS) -c $< -o $@ 

%.o: %.c
	gcc $(CFLAGS) -c  $< -o $@ 

all: 
	gcc $(CFLAGS) -c queue.c -o queue.o 
	gcc $(CFLAGS) -c csapp.c -lpthread
	# gcc $(CFLAGS) $(INCFLAGS) -c  ./client/client.c -o ./client/client.o csapp.o -lpthread
	gcc $(CFLAGS) $(INCFLAGS) ./client/client.c -o ./client/client  $(LFLAGS) csapp.o -lpthread
	gcc $(CFLAGS) ./server/server.c -o ./server/server queue.o csapp.o -lpthread
	



clean:
	rm -f  *.o
	rm -f ./server/*.o
	rm -f ./client/*.o
	rm -f ./client/client
	rm -f ./server/server

	

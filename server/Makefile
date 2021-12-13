CFLAGS = -g -Wall -Wvla -fsanitize=address
server: server.o 
	gcc $(CFLAGS) -o $@ $^ ../queue.o ../csapp.o -lpthread
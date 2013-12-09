CC=gcc
CFLAGS=-c -o2 -std=c99

all: client_PFS server_PFS

client_PFS: client_PFS.o
		$(CC) client_PFS.o -o client_PFS

client_PFS.o: client_PFS.c
		$(CC) $(CFLAGS) client_PFS.c -o client_PFS.o

server_PFS: server_PFS.o
		$(CC) server_PFS.o -o server_PFS

server_PFS.o: server_PFS.c
		$(CC) $(CFLAGS) server_PFS.c -o server_PFS.o

clean:
		rm -rf *o server_PFS client_PFS

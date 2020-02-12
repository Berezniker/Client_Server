CC = gcc
CFLAGS = -g -Wall
SERVER = server
CLIENT = client

create: $(SERVER)
	rlwrap ./$(SERVER)

$(SERVER): $(SERVER).c database.o workflow.o
	$(CC) $(CFLAGS) -o $(SERVER) $^

database.o: database.c database.h
	$(CC) $(CFLAGS) -c $<

workflow.o: workflow.c workflow.h database.o
	$(CC) $(CFLAGS) -c $<

valsrv: $(SERVER)
	valgrind ./server


connect: $(CLIENT)
	rlwrap ./$(CLIENT)

$(CLIENT): $(CLIENT).c
	$(CC) $(CFLAGS) -o $(CLIENT) $^

valcln: $(CLIENT)
	valgrind ./client


clean:
	rm -f $(SERVER) $(CLIENT) *.o
all: server
server: server.c
	gcc send_file.c get_file.c first_aid.c server.c -levent -lcurl -ljansson -pthread -o server
clean:
	rm server

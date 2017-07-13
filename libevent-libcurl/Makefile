all: server
server: server.c
	gcc send_file.c get_file.c server.c -levent -lcurl -ljansson -o server
clean:
	rm server

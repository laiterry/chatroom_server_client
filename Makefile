CC=gcc

linux:
	${CC} server.c -o server -lpthread
	${CC} client.c -o client -lpthread

unix:
	${CC} server.c -o server -lnsl -lsocket -lpthread
	${CC} client.c -o client -lnsl -lsocket -lpthread

clean:
	rm server client *~

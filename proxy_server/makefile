FILES := server.c udp.c atem_connection.c ../src/atem.c cache.c

server: $(FILES) ../src/atem.h atem_connection.h udp_unix.h udp.h
	gcc $(FILES) -o server

clean:
	rm server

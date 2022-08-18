FILES := server.c udp.c timer.c cache.c relay.c ../src/atem.c

release:
	gcc $(FILES) main.c -o server -O3

dev:
	gcc $(FILES) main.c -o server -Wall -g -DDEBUG

clean:
	rm server

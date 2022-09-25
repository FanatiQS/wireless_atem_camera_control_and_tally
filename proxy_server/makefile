# Select compiler based on platform being windows with nmake or unix with gcc
#\
!ifndef 0 #\
CC = cl #\
!else
CC = gcc
#\
endif

FILES = server.c udp.c timer.c cache.c relay.c ../src/atem.c

release:
	$(CC) $(FILES) main.c -o server -O3

dev:
	$(CC) $(FILES) main.c -o server -Wall -g -DDEBUG

clean:
	rm server

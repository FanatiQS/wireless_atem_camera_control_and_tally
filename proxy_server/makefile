# The directory everything should be output to
BUILD_DIR = build

FILES_SOURCE = server.c udp.c timer.c cache.c relay.c ../src/atem.c
FILES_HEADER = server.h udp.h timer.h cache.h relay.h ../src/atem.h atem_extra.h debug.h udp_unix.h udp_win.h
FILES_EXEC = main.c $(FILES_SOURCE)
FILES_ALL = makefile $(FILES_EXEC) $(FILES_HEADER)

FLAGS_ERROR = -Weverything -Wno-pedantic -Wno-implicit-int-conversion -Wno-sign-conversion -Wno-missing-prototypes -Wno-unused-parameter -Wno-\#warnings -Wno-cast-align -Wno-padded

FLAGS_RELEASE = -O2
FLAGS_DEBUG = -DDEBUG -g -Wall -pedantic

COMPILE_RELEASE = $(CC) $(FILES_EXEC) $(FLAGS_RELEASE) $(CFLAGS)
COMPILE_DEBUG = $(CC) $(FILES_EXEC) $(FLAGS_DEBUG) $(CFLAGS)

NAME_RELEASE = $(BUILD_DIR)/proxy_release
NAME_DEBUG = $(BUILD_DIR)/proxy_debug



# Windows rules for NMAKE (works since nmake doesn't see line continuation from back slash)\
!ifndef 0 #\
release: $(NAME_RELEASE).exe #\
debug: $(NAME_DEBUG).exe #\
clean: clean_win #\
!else
# Non-Windows rules for GNU MAKE
release: $(NAME_RELEASE)
debug: $(NAME_DEBUG)
clean: clean_unix
#\
!endif

# Shared rules to execute for all
all: release debug

# Compiles release executable for non-windows
$(NAME_RELEASE): $(FILES_ALL)
	$(COMPILE_RELEASE) -o $@

# Compiles release executable for windows
$(NAME_RELEASE).exe: $(FILES_ALL)
	$(COMPILE_RELEASE) /Fe:$@

# Compiles debug executable for non-windows
$(NAME_DEBUG): $(FILES_ALL)
	$(COMPILE_DEBUG) -o $@

# Compiles debug executable for windows
$(NAME_DEBUG).exe: $(FILES_ALL)
	$(COMPILE_DEBUG) /Fe:$@

# Cleans up non-windows build files
clean_unix:
	$(RM) $(NAME_RELEASE) $(NAME_DEBUG) $(NAME_LIB_CF).a $(NAME_LIB_CF).h
	rm -r $(NAME_DEBUG).dSYM

# Cleans up windows build files
clean_win:
	del $(NAME_RELEASE).exe $(NAME_DEBUG).exe *.obj ../src/atem.obj




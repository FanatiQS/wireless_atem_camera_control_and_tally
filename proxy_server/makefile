# The directory used for temporary files and where everything should be output to
BUILD_DIR = build
OUT_DIR = $(BUILD_DIR)

NAME_RELEASE = $(OUT_DIR)/proxy_release
NAME_DEBUG = $(OUT_DIR)/proxy_debug
NAME_LIB_CF = $(OUT_DIR)/atem_server_cf

# Windows aliases for NMAKE (works since nmake doesn't see line continuation from back slash)\
!ifndef 0 #\
release: $(NAME_RELEASE).exe #\
debug: $(NAME_DEBUG).exe #\
clean: clean_win #\
!else
# Non-Windows aliases for GNU MAKE
release: $(NAME_RELEASE)
debug: $(NAME_DEBUG)
clean: clean_unix
all: lib_cf
#\
!endif

# Shared rules to execute for all
all: release debug

# CoreFoundation build alias
lib_cf: $(NAME_LIB_CF).a



FILES_SOURCE = server.c timer.c cache.c relay.c ../src/atem.c
FILES_HEADER = server.h udp.h timer.h cache.h relay.h ../src/atem.h atem_extra.h debug.h udp_unix.h udp_win.h
FILES_ALL = makefile $(FILES_SOURCE) $(FILES_HEADER)

FILES_EXEC = main.c $(FILES_SOURCE)
FILES_EXEC_ALL = main.c $(FILES_ALL)

FLAGS_ERROR = -Weverything -Wno-pedantic -Wno-implicit-int-conversion -Wno-sign-conversion -Wno-missing-prototypes -Wno-unused-parameter -Wno-\#warnings -Wno-cast-align -Wno-padded

FLAGS_DEFAULT = -Wall
FLAGS_DEFAULT_UNIX = $(FLAGS_DEFAULT) -pedantic -Wextra -fvisibility=hidden -std=c11 -o $@
FLAGS_DEFAULT_WIN = $(FLAGS_DEFAULT) -std:c11 -Fo$(BUILD_DIR)\ -Fe:$@

FLAGS_RELEASE = -O2
FLAGS_DEBUG = -DDEBUG



# Creates directory for output if it does not exist
$(BUILD_DIR):
	mkdir $@

# Compiles release executable for non-windows
$(NAME_RELEASE): $(FILES_EXEC_ALL)
	$(CC) $(FILES_EXEC) $(FLAGS_DEFAULT_UNIX) $(FLAGS_RELEASE) -flto -Ofast $(CFLAGS)

# Compiles release executable for windows
$(NAME_RELEASE).exe: $(FILES_EXEC_ALL) $(BUILD_DIR)
	$(CC) $(FILES_EXEC) $(FLAGS_DEFAULT_WIN) $(FLAGS_RELEASE) -GL -Gw $(CFLAGS)

# Compiles debug executable for non-windows
$(NAME_DEBUG): $(FILES_EXEC_ALL)
	$(CC) $(FILES_EXEC) $(FLAGS_DEFAULT_UNIX) $(FLAGS_DEBUG) -g $(CFLAGS)

# Compiles debug executable for windows
$(NAME_DEBUG).exe: $(FILES_EXEC_ALL) $(BUILD_DIR)
	$(CC) $(FILES_EXEC) $(FLAGS_DEFAULT_WIN) $(FLAGS_DEBUG) -Z7 $(CFLAGS)

# Cleans up non-windows build files
clean_unix:
	$(RM) $(NAME_RELEASE) $(NAME_DEBUG) $(NAME_LIB_CF).a $(NAME_LIB_CF).h $(BUILD_DIR)/*.o
	rm -fr $(NAME_DEBUG).dSYM

# Cleans up windows build files
clean_win:
	del $(NAME_RELEASE).exe $(NAME_DEBUG).exe $(BUILD_DIR)/*.obj



# Compiles object files for static library dependency
NAME_LINKED = $(BUILD_DIR)/linked.o
$(NAME_LINKED): $(FILES_ALL) bindings/binding_cf.c $(BUILD_DIR)
	for f in bindings/binding_cf.c $(FILES_SOURCE); do\
		$(CC) $(FLAGS_DEFAULT_UNIX) $(FLAGS_RELEASE) -Ofast -c $${f} -o $(BUILD_DIR)/`basename $${f} .c`_tmp.o $(CFLAGS);\
	done
	ld -r $(BUILD_DIR)/*_tmp.o -o $@
	$(RM) $(BUILD_DIR)/*_tmp.o

# CoreFoundation build static library
$(NAME_LIB_CF).a: $(NAME_LINKED) $(NAME_LIB_CF).h
	ar -crs $@ $<
	$(RM) $(NAME_LINKED)

# CoreFoundation copy header
$(NAME_LIB_CF).h: bindings/binding_cf.h
	cp $< $@

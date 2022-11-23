# The directory everything should be output to
BUILD_DIR = build

FILES_SOURCE = server.c udp.c timer.c cache.c relay.c ../src/atem.c
FILES_HEADER = server.h udp.h timer.h cache.h relay.h ../src/atem.h atem_extra.h debug.h udp_unix.h udp_win.h
FILES_ALL = makefile $(BUILD_DIR) $(FILES_SOURCE) $(FILES_HEADER)
FILES_EXEC = main.c $(FILES_SOURCE)
FILES_EXEC_ALL = main.c $(FILES_ALL)

FLAGS_ERROR = -Weverything -Wno-pedantic -Wno-implicit-int-conversion -Wno-sign-conversion -Wno-missing-prototypes -Wno-unused-parameter -Wno-\#warnings -Wno-cast-align -Wno-padded

FLAGS_DEFAULT = -Wall -pedantic -Wextra
FLAGS_RELEASE = $(FLAGS_DEFAULT) -O2
FLAGS_DEBUG = $(FLAGS_DEFAULT) -DDEBUG -g

COMPILE_RELEASE = $(CC) $(FILES_EXEC) $(FLAGS_RELEASE) $(CFLAGS)
COMPILE_DEBUG = $(CC) $(FILES_EXEC) $(FLAGS_DEBUG) $(CFLAGS)
COMPILE_LIB_CF = $(CC) $(FLAGS_RELEASE) -fvisibility=hidden -c $(CFLAGS)

NAME_RELEASE = $(BUILD_DIR)/proxy_release
NAME_DEBUG = $(BUILD_DIR)/proxy_debug
NAME_LIB_CF = $(BUILD_DIR)/atem_server_cf



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
all: lib_cf
#\
!endif

# Shared rules to execute for all
all: release debug

# Compiles release executable for non-windows
$(NAME_RELEASE): $(FILES_EXEC_ALL)
	$(COMPILE_RELEASE) -o $@

# Compiles release executable for windows
$(NAME_RELEASE).exe: $(FILES_EXEC_ALL)
	$(COMPILE_RELEASE) /Fe:$@

# Compiles debug executable for non-windows
$(NAME_DEBUG): $(FILES_EXEC_ALL)
	$(COMPILE_DEBUG) -o $@

# Compiles debug executable for windows
$(NAME_DEBUG).exe: $(FILES_EXEC_ALL)
	$(COMPILE_DEBUG) /Fe:$@

# Cleans up non-windows build files
clean_unix:
	$(RM) $(NAME_RELEASE) $(NAME_DEBUG) $(NAME_LIB_CF).a $(NAME_LIB_CF).h
	rm -fr $(NAME_DEBUG).dSYM

# Cleans up windows build files
clean_win:
	del $(NAME_RELEASE).exe $(NAME_DEBUG).exe *.obj ../src/atem.obj

# Creates directory for output if it does not exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)



# CoreFoundation build alias
lib_cf: $(NAME_LIB_CF).a

# CoreFoundation build rule
$(NAME_LIB_CF).a: $(FILES_ALL) bindings/binding_cf.c bindings/binding_cf.h
	$(COMPILE_LIB_CF) server.c -o $(BUILD_DIR)/server.o
	$(COMPILE_LIB_CF) udp.c -o $(BUILD_DIR)/udp.o
	$(COMPILE_LIB_CF) timer.c -o $(BUILD_DIR)/timer.o
	$(COMPILE_LIB_CF) cache.c -o $(BUILD_DIR)/cache.o
	$(COMPILE_LIB_CF) relay.c -o $(BUILD_DIR)/relay.o
	$(COMPILE_LIB_CF) ../src/atem.c -o $(BUILD_DIR)/atem.o
	$(COMPILE_LIB_CF) bindings/binding_cf.c -o $(BUILD_DIR)/binding_cf.o
	ar -crs $@ $(BUILD_DIR)/*.o
	cp bindings/binding_cf.h $*.h
	$(RM) $(BUILD_DIR)/*.o

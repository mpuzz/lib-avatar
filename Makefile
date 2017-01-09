CC = g++ # C compiler
INC = -Iinclude $(shell python-config --cflags)
CFLAGS = -std=c++11 -fPIC -Wall -Wextra -g3 $(INC) # C flags
LDFLAGS = -shared -lrt # linking flags
RM = rm -rf  # rm command
DST_DIR = lib
TARGET_LIB = avatar_qemu.so # target lib

SRCS = libavatar.c # source files
OBJS = $(SRCS:.c=.o)

.PHONY: all
all: ${TARGET_LIB}

$(DST_DIR):
	@mkdir $@

$(TARGET_LIB): $(SRCS) $(DST_DIR)
	$(CC) $(INC) $(CFLAGS) -o $(DST_DIR)/$@ $(SRCS) $(LDFLAGS)

.PHONY: clean
clean:
	-${RM} $(DST_DIR) ${OBJS} $(SRCS:.c=.d)

.PHONY: install
prefix=/usr
install: ${TARGET_LIB}
	install -m 0755 $(DST_DIR)/${TARGET_LIB} $(prefix)/lib

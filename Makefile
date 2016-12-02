CC = g++ # C compiler
INC = -Iinclude
CFLAGS = -std=c++11 -fPIC -Wall -Wextra -O2 -g $(INC) # C flags
LDFLAGS = -shared  # linking flags
RM = rm -f  # rm command
TARGET_LIB = libavatar.so # target lib

SRCS = libavatar.c # source files
OBJS = $(SRCS:.c=.o)

.PHONY: all
all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS)
	$(CC) ${LDFLAGS} -o $@ $^

$(SRCS:.c=.d):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@

include $(SRCS:.c=.d)

.PHONY: clean
clean:
	-${RM} ${TARGET_LIB} ${OBJS} $(SRCS:.c=.d)

.PHONY: install
prefix=/usr
install: ${TARGET_LIB}
	install -m 0755 ${TARGET_LIB} $(prefix)/lib

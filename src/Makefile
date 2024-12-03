EXEC = tema1

SRCS = main.c mapper.c reducer.c utils.c
OBJS = $(SRCS:.c=.o)

CC = gcc
CFLAGS = -Wall -Werror -pthread

all: build

build: $(OBJS)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXEC)
	rm -f *.txt

# build:
# 		gcc main.c -o tema1 -lpthread -Wall -Werror
# clean:
# 		rm tema1

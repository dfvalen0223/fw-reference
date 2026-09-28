# Host unit tests for the ESP-IDF-independent firmware code.
#   make -C led-letters/firmware/test
CC ?= cc
CFLAGS ?= -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
SRC = test_fx.c ../main/effects.c ../main/power.c

test_fx: $(SRC) ../main/effects.h ../main/power.h ../main/led_layout.h
	$(CC) $(CFLAGS) -I../main -o $@ $(SRC) -lm

.PHONY: test clean
test: test_fx
	./test_fx

.DEFAULT_GOAL := test

clean:
	rm -f test_fx

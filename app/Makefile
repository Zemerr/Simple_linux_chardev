CC=gcc
DEPS = ../driver
CFLAGS=-Wall -I $(DEPS)
OBJ = device_app.c

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

device_app: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	-rm -f *.o device_app
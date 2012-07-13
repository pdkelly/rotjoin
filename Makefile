TARGET = rotjoin

all: $(TARGET)

PREFIX = /usr/local

CC = gcc -O2
CFLAGS = -Wall 
LDFLAGS = 

DEPS = rotjoin.h
INCLUDES =
LIBS = -lsndfile

OBJS = rotjoin.o splice.o

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

install: $(TARGET)
	-mkdir -p $(PREFIX)/bin
	install $(TARGET) $(PREFIX)/bin

clean:
	rm -f $(OBJS) $(TARGET)

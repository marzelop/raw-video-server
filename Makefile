CC=gcc
SRCDIR=src
OBJDIR=build
CFLAGS=-Wall -O3
LFLAGS=
DFLAGS=-g -D_DEBUG_
CLIFLAGS=
OBJNAMES=protocol.o frame.o timeop.o
OBJS=$(foreach OBJ, $(OBJNAMES),  $(OBJDIR)/$(OBJ))

CFLAGS += $(DFLAGS)

.SECONDARY:

all: server client

# debug: CFLAGS += $(DFLAGS)
# debug: all

.PHONY: clean purge

%: $(OBJS) $(OBJDIR)/%.o
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS) $(CLIFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/%.h
	$(CC) $(CFLAGS) -c $< -o $@ $(CLIFLAGS)

clean:
	rm -rf $(OBJDIR)/*.o

purge: clean
	rm -f client server

CXX = gcc
LDFLAGS = -lpthread -lpulse -lpulse-simple

main: 
	  $(CXX) server.c -o server $(LDFLAGS)
	  $(CXX) client.c -o client $(LDFLAGS)

clean:
	rm -f *.o server client

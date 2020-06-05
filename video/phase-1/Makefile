CXX = g++ -std=c++11
obj_server = rsa.o server.o
obj_client = rsa.o client.o
OPENCV_FLAGS = `pkg-config --cflags --libs opencv`
LDFLAGS = -lpthread -lpulse -lpulse-simple

rsa.o:  rsa.cpp
		$(CXX) -c rsa.cpp

main: $(obj_server) $(obj_client) rsa.o
	  $(CXX) $(obj_server) -o server $(OPENCV_FLAGS) $(LDFLAGS)
	  $(CXX) $(obj_client) -o client $(OPENCV_FLAGS) $(LDFLAGS)

server.o: rsa.o
	   $(CXX) -c server.cpp

client.o: rsa.o
	   $(CXX) -c client.cpp

.PHONY: clean

clean:
	rm -f *.o server client
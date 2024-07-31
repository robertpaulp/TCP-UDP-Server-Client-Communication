CFLAGS = -Wall -Wextra -g -Wno-error=unused-variable
CINCLUDES = -I include/
LDFLAGS = -lm

all: server subscriber

lib/libserver.o: lib/libserver.cpp include/libserver.hpp

lib/libsubscriber.o: lib/libsubscriber.cpp include/libsubscriber.hpp

server: server.cpp lib/libserver.o
	g++ $(CFLAGS) $(CINCLUDES) server.cpp lib/libserver.o -o server $(LDFLAGS)

subscriber: subscriber.cpp lib/libsubscriber.o
	g++ $(CFLAGS) $(CINCLUDES) subscriber.cpp lib/libsubscriber.o -o subscriber

.cpp.o:
	g++ $(CINCLUDES) $(CFLAGS) -g -c $< -o $@

run_server: server
	./server 8080

run_subscriber: subscriber
	./subscriber 1 0.0.0.0 8080

clean:
	rm -f server *.o lib/*.o subscriber
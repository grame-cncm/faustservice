objects = main.o server.o utilities.o

faustserver : $(objects)
	g++ -lboost_random -lboost_system -lboost_filesystem -lmicrohttpd -o faustserver $(objects)

main.o : main.cpp
	g++ -c main.cpp

server.o : server.cpp server.hh
	g++ -c server.cpp

utilities.o : utilities.cpp utilities.hh
	g++ -c utilities.cpp

.PHONY : clean
clean :
	-rm faustserver $(objects)

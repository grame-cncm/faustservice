all: client.cpp main.cpp FaustWebClient.h FaustWebClient.cpp
	g++ -I/usr/local/include -L/usr/local/lib main.cpp FaustWebClient.cpp JsonParser.cpp -lcurl -o faustwebtest
	g++ -I/usr/local/include -L/usr/local/lib client.cpp FaustWebClient.cpp JsonParser.cpp -lcurl -o faustwebclient

install:

	cp faustwebclient /usr/local/bin

clean:

	rm -f faustwebclient faustwebtest


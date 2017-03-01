all: client.cpp main.cpp ../FaustWebClient.h ../FaustWebClient.cpp
	g++ -I/usr/local/include -L/usr/local/lib -lcurl main.cpp ../FaustWebClient.cpp ../JsonParser.cpp -o faustwebtest
	g++ -I/usr/local/include -L/usr/local/lib -lcurl client.cpp ../FaustWebClient.cpp ../JsonParser.cpp -o faustwebclient

install:

	cp faustwebclient /usr/local/bin

clean:

	rm -f faustwebclient faustwebtest


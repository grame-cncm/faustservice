#----------------------------------------------------------------------------------------
# Faustweb requires : boost, libmicrohttpd, libcrypto and libarchive
# in order to compile it just do make
#----------------------------------------------------------------------------------------
dest   := /usr/local/bin

faustweb: *.cpp *.hh
	g++ -g -O1 -I/opt/local/include *.cpp \
	-L/opt/local/lib/ \
	-lmicrohttpd -lboost_filesystem -lboost_system -lboost_program_options \
	-larchive -lcrypto -lm \
	-o faustweb
	
clean :
	rm faustweb

format :
	astyle *.cpp *.hh
	
install:
	install faustweb $(dest)
	install bin/remoteOSX $(dest)
		
uninstall:
	rm -f  $(dest)/faustweb
	rm -f  $(dest)/remoteOSX 

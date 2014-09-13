#----------------------------------------------------------------------------------------
# Faustweb requires : boost, libmicrohttpd, libcrypto and libarchive
# in order to compile it just do make
#----------------------------------------------------------------------------------------
dest   := /usr/local/bin
pdsdk  := /usr/include/pd

faustweb: *.cpp *.hh
	g++ -g -O1 -I/opt/local/include *.cpp \
	-I/usr/local/Cellar/libarchive/3.1.2/include/ \
	-L/usr/local/Cellar/libarchive/3.1.2/lib/ \
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
	install -d $(pdsdk)/pdfaustextra/
	install pdfaustextra/*.pd $(pdsdk)/pdfaustextra/
		
uninstall:
	rm -f  $(dest)/faustweb
	rm -f  $(dest)/remoteOSX 

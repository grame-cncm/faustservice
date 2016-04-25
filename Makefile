#----------------------------------------------------------------------------------------
# Faustweb requires : boost, libmicrohttpd, libcrypto and libarchive
# in order to compile it just do make
#----------------------------------------------------------------------------------------
dest   := /usr/local/bin
pdsdk  := /usr/include/pd

ifeq ($(shell uname -s), Darwin)
EXT = -mt
else
EXT = ""
endif

faustweb: *.cpp *.hh
	g++ $(CXXFLAGS) $(LDFLAGS) *.cpp \
	-lmicrohttpd -lboost_filesystem$(EXT) -lboost_system$(EXT) -lboost_program_options$(EXT) \
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

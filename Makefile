#----------------------------------------------------------------------------------------
# Faustweb requires : boost, libmicrohttpd, libcrypto and libarchive
# in order to compile it just do make
# Have a look at faustweb.conf for upstart. It contains a path to faustweb executable
# that you will have to adapt
#----------------------------------------------------------------------------------------
dest   := /usr/local/bin
pdsdk  := /usr/include/pd

ifeq ($(shell uname -s), Darwin)
EXT = -mt
LDFLAGS = -L/opt/local/lib
CXXFLAGS = -Wall -Wno-unused-local-typedef -O3 -I/opt/local/include
else
EXT = ""
CXXFLAGS = -Wall -Wno-unused-local-typedef -O3
endif

faustweb: *.cpp *.hh
	g++ $(CXXFLAGS) *.cpp $(LDFLAGS) \
	-lmicrohttpd -lboost_filesystem$(EXT) -lboost_system$(EXT) -lboost_program_options$(EXT) \
	-larchive -lcrypto -lm \
	-o faustweb
	
clean :
	rm faustweb

format :
	astyle *.cpp *.hh
	
install:
	install upstart-faustweb.conf /etc/init/faustweb.conf
	install apache2-faustweb.conf /etc/apache2/sites-available/002-faustweb.conf
    install faustweb $(dest)
	install bin/remoteOSX $(dest)
	install -d $(pdsdk)/pdfaustextra/
	install pdfaustextra/*.pd $(pdsdk)/pdfaustextra/
		
uninstall:
	rm -f  $(dest)/faustweb
	rm -f  $(dest)/remoteOSX 

start:
	initctl start faustweb


help:
	echo "faustweb : compiles faustweb"
	echo "clean    : remove faustweb"
	echo "format   : format source code using astyle"
	echo "install  : install upstart config and other files"
	echo "uninstall: "
	echo "start    : starts faustweb as an upstart service"

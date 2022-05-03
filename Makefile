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
CXXFLAGS = -Wall -Wextra -Wno-unused-local-typedef -O3 -I/opt/local/include
CLANGVERSION =
CXX=clang++$(CLANGVERSION)
STD=c++11
else
EXT = ""
CXXFLAGS = -Wall -Wextra -O3 -DBOOST_NO_CXX11_SCOPED_ENUMS
STD=c++11
endif

all : faustweb

faustweb : *.cpp *.hh
	$(CXX) -std=$(STD) $(CXXFLAGS) *.cpp $(LDFLAGS) \
	-lmicrohttpd -lboost_filesystem$(EXT) -lboost_system$(EXT) -lboost_program_options$(EXT) \
	-larchive -lcrypto -lm \
	-o faustweb

clean :
	rm faustweb

format :
	clang-format$(CLANGVERSION) -i -style=file *.cpp *.hh

install_systemd:
	install faustweb.service /etc/systemd/system/faustweb.service
	sed -i s%HOMEPATH%$(HOME)%g /etc/systemd/system/faustweb.service
	systemctl enable faustweb
	rm -rf sessions
	systemctl start faustweb
	
# faustservice should be run from /home/faust/Install/faustservice/faustweb
# this is hard coded into faustweb.service
install:
	install faustweb.conf /etc/init/faustweb.conf
	install apache2-faustweb.conf /etc/apache2/sites-available/002-faustweb.conf
	install bin/remoteOSX $(dest)
	install -d $(pdsdk)/pdfaustextra/
	install pdfaustextra/*.pd $(pdsdk)/pdfaustextra/
		
uninstall:
	rm -f  $(dest)/remoteOSX 
	rm -rf $(pdsdk)/pdfaustextra/

start:
	initctl start faustweb

test:
	./faustweb -v 1 -p 80 -d /tmp/sessions -r $(shell pwd)/faustweb


help:
	@echo "faustweb : compiles faustweb"
	@echo "clean    : remove faustweb"
	@echo "format   : format source code using astyle"
	@echo "install  : install upstart config and other files"
	@echo "uninstall: "
	@echo "start    : starts faustweb as an upstart service"
	@echo "test     : starts faustweb for tests"

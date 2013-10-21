#----------------------------------------------------------------------------------------
# Faustweb requires : boost, libmicrohttpd, libcrypto and libarchive
# in order to compile it just do make
#----------------------------------------------------------------------------------------

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
	
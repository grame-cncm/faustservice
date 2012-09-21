ACLOCAL_AMFLAGS = -I .

bin_PROGRAMS = faustserver
faustserver_SOURCES = src/main.cpp src/server.cpp
faustserver_CPPFLAGS = $(BOOST_CPPFLAGS)
faustserver_LDFLAGS = $(BOOST_THREAD_LDFLAGS)
faustserver_LIBS = $(BOOST_THREAD_LIBS)
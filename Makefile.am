ACLOCAL_AMFLAGS = -I .

bin_PROGRAMS = faustserver
faustserver_SOURCES = main.cpp server.cpp utilities.cpp
faustserver_CPPFLAGS = $(BOOST_CPPFLAGS)
faustserver_LDFLAGS = $(BOOST_THREAD_LDFLAGS)
faustserver_LIBS = $(BOOST_THREAD_LIBS)
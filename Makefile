#----------------------------------------------------------------------------------------
# Faustweb requires : boost, libmicrohttpd, libcrypto and libarchive
#
# Use `make` to build faustweb
# and `make help` for other targets
#----------------------------------------------------------------------------------------

dest   := /usr/local/bin
pdsdk  := /usr/include/pd

# Platform detection
ifeq ($(shell uname -s), Darwin)
    EXT = -mt
    LDFLAGS = -L/opt/local/lib
    CXXFLAGS = -Wall -Wextra -Wno-unused-local-typedef -O3 -I/opt/local/include
    CLANGVERSION =
    CXX = clang++$(CLANGVERSION)
    STD = c++11
else
    EXT =
    CXXFLAGS = -Wall -Wextra -O3 -DBOOST_NO_CXX11_SCOPED_ENUMS
    STD = c++11
endif

# Source files
SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)
DEPS = $(SOURCES:.cpp=.d)
TARGET = faustweb

# Libraries
LIBS = -lmicrohttpd -lboost_filesystem$(EXT) -lboost_system$(EXT) \
       -lboost_program_options$(EXT) -larchive -lcrypto -lm

# Default target
all: $(TARGET)

# Link object files
$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ $(LIBS) -o $@

# Compile source files to object files
%.o: %.cpp
	$(CXX) -std=$(STD) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Include dependency files
-include $(DEPS)

# Clean build artifacts
clean:
	rm -f $(TARGET) $(OBJECTS) $(DEPS)

# Format code
format:
	clang-format$(CLANGVERSION) -i -style=file *.cpp *.hh

# Installation targets
install_systemd:
	install faustweb.service /etc/systemd/system/faustweb.service
	sed -i s%HOMEPATH%$(HOME)%g /etc/systemd/system/faustweb.service
	systemctl enable faustweb
	rm -rf sessions
	systemctl start faustweb

install:
	install faustweb.conf /etc/init/faustweb.conf
	install apache2-faustweb.conf /etc/apache2/sites-available/002-faustweb.conf
	install bin/remoteOSX $(dest)
	install -d $(pdsdk)/pdfaustextra/
	install pdfaustextra/*.pd $(pdsdk)/pdfaustextra/

uninstall:
	rm -f $(dest)/remoteOSX 
	rm -rf $(pdsdk)/pdfaustextra/

start:
	initctl start faustweb

test:
	./faustweb -v 3 -p 80 -d /tmp/sessions -r $(shell pwd)/faustweb

help:
	@echo "Targets:"
	@echo "  all       : Build faustweb (default)"
	@echo "  clean     : Remove build artifacts"
	@echo "  format    : Format source code using clang-format"
	@echo "  install   : Install upstart config and other files"
	@echo "  uninstall : Remove installed files"
	@echo "  start     : Start faustweb as an upstart service"
	@echo "  test      : Start faustweb for testing"

.PHONY: all clean format install install_systemd uninstall start test help
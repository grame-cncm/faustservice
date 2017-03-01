
# FaustWeb client

The goal of this project is to create a client that communicates with a FaustWeb compilation service, to export Faust programs as binary applications or plugins.

## Library dependancy

- `libcurl` : http://curl.haxx.se/libcurl/

## Compilation

- `make && sudo make install` : to compile and install the `faustwebclient` binary

## Options

The compiled binary is : faustwebclient [-service] [-url <...>] [-platform <...>] [-target <...>] <file.dsp>

The following options are available: 

- `-service`            : to print all available platform/targets on the service URL (default 'http://faustservice.grame.fr')
- `-url <...>`          : to specify service URL (default 'http://faustservice.grame.fr')
- `-platform <...>`     : to specify compilation platform
- `-target <...>`       : to specify compilation target for the chosen platform


## Example

- `faustwebclient -platform osx -target max-msp test.dsp`

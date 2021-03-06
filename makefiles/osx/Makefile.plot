################################################################################
#							FaustWeb OSX script
#  The behaviour of this Makefile will only be correct if
#  only one .dsp file exists in the folder
################################################################################

faustfile ?= $(wildcard *.dsp)
libraries	= $(wildcard *.lib)

binary.zip : $(faustfile)
	remoteOSX faust2plot $(faustfile) $(libraries) $(OPT)

src.cpp : $(faustfile)
	faust -i -a matlabplot.cpp $(faustfile) -o src.cpp

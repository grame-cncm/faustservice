################################################################################
#							FaustWeb OSX script
#  The behaviour of this Makefile will only be correct if
#  only one .dsp file exists in the folder
################################################################################

faustfile	?= $(wildcard *.dsp)

binary.zip : $(faustfile)
	faust2plot $(OPT) $(faustfile)
	zip -r binary.zip $(faustfile:.dsp=)

src.cpp : $(faustfile)
	faust -i -a matlabplot.cpp $(faustfile) -o src.cpp

################################################################################
#							FaustWeb OSX script
#  The behaviour of this Makefile will only be correct if
#  only one .dsp file exists in the folder
################################################################################

faustfile	?= $(wildcard *.dsp)

binary.zip : $(faustfile)
	faust -lang c $(faustfile) -o $(faustfile:.dsp=.c)
	zip -r binary.zip $(faustfile) $(faustfile:.dsp=.c)

src.cpp : $(faustfile)
	faust -i -a minimal.c $(faustfile) -o src.c

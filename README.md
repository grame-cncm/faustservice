
### FaustWeb ###

The goal of this project is to develop a Web API for the Faust compiler allowing to do remote compilations.

#### Required libraries ####

In order to compile FaustWeb links against the following libraries:

 - **libarchive** (sudo apt-get install libarchive-dev)
 - **boost** (sudo apt-get install libboost-filesystem-dev  libboost-program-options-dev)
 - **libcrypto** (sudo apt-get install libcrypto++-dev)
 - **libmicrohttpd**

Make sure to download the current development trunk of all these libraries. Then, the standard:

 - **mkdir build && cd build**
 - **../configure**
 - **make**
 - **sudo make install**

#### To use FaustWeb ####

 - **./faustweb** starts FaustWeb on port 8888

#### From a browser at URL http://localhost:8888/ ####

Select a file kisana.dsp and send it. The sent file can be a either a .dsp or an .zip archive containing a .dsp .lib files. Next use the returned <key> for example cf55531c580cc7d3485a5161259f0571d3e6bdef

#### General API ####

 - http://localhost:8888/\<key\>/src.cpp	returns the 'src.cpp' file containing the cpp source code 
 - http://localhost:8888/\<key\>/svg.zip	returns a 'svg.zip' archive containing a svg folder with all .svg files
 - http://localhost:8888/\<key\>/mdoc.zip returns a ’mdoc.zip' archive  containing a kisana-mdoc folder with automatic documentation

#### API for each architecture ####

 - http://localhost:8888/\<key\>/\<plateform\>/\<architecture\>/binary.zip returns the 'binary.zip' archive containing one or several binairies
 - http://localhost:8888/\<key\>/\<plateform\>/\<architecture\>/src.cpp	returns the architecture wrapped cpp source code 

#### List of available architectures for each platform ####

For OSX :
	osx/coreaudio-qt	
	osx/jack-qt				
	osx/supercollider	
	osx/vsti
	osx/csound		
	osx/max-msp		
	osx/puredata	
	osx/vst

#### Building a port with curl ####

curl -F'file=@"/Users/yannorlarey/Develop/faust/examples/highShelf.dsp";filename="highShelf.dsp"'  http://localhost:8080/filepost

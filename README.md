
### FaustWeb ###

The goal of this project is to develop a Web API for the Faust compiler allowing to do remote compilations.

#### Required libraries ####

In order to compile FaustWeb links against the following libraries:

 - **libarchive** (sudo apt-get install libarchive-dev)
 - **boost** (sudo apt-get install libboost-filesystem-dev  libboost-program-options-dev)
 - **libcrypto** (sudo apt-get install libcrypto++-dev)
 - **libmicrohttpd** (sudo apt-get install libmicrohttpd-dev)

Make sure to download the current development trunk of all these libraries and install. Then, the standard:

 - **make**
 - **sudo make install**

#### To use FaustWeb ####

 - **./faustweb** starts FaustWeb on port 8888

#### From a browser at URL http://localhost:8888/ ####

Select a file *kisana.dsp* and send it. The sent file can be a either a .dsp or an .zip archive containing a .dsp .lib files. Next use the returned <key> for example cf55531c580cc7d3485a5161259f0571d3e6bdef

#### General API ####

 - http://localhost:8888/<key\>/src.cpp returns the 'src.cpp' file containing the cpp source code 
 - http://localhost:8888/<key\>/svg.zip returns a 'svg.zip' archive containing a svg folder with all .svg files
 - http://localhost:8888/<key\>/mdoc.zip returns a ’mdoc.zip' archive  containing a kisana-mdoc folder with automatic documentation

#### API for each architecture ####

 - http://localhost:8888/<key\>/\<plateform\>/\<architecture\>/binary.zip returns the 'binary.zip' archive containing one or several binairies
 - http://localhost:8888/<key\>/\<plateform\>/\<architecture\>/src.cpp	returns the architecture wrapped cpp source code 

#### List of available architectures for each platform (here of OSX as en example) ####

For OSX :
 osx/coreaudio-qt	
 osx/coreaudio-qt-midi  
 osx/csound
 osx/jack-qt    
 osx/max
 osx/plot
 osx/puredata
 osx/puredata-poly
 osx/supercollider	
 osx/vst
 osx/vsti

#### Testing the service using Curl ####

The local service run on the http://localhost:8888/ URL, and can be tested like in the following examples:

	curl http://localhost:8888/targets
	curl -F'file=@"clarinet.dsp";filename="clarinet.dsp"'  http://localhost:8888/filepost
	returned SHA key: 5ADBDAF2AFFF8387F4FCB9F05BA84E374DE3ABAF
	curl http://localhost:8888/5ADBDAF2AFFF8387F4FCB9F05BA84E374DE3ABAF/android/smartkeyb/binary.apk --output binary.apk

#### Direct compilation

Or with a single complete command:

	curl -F'file=@"clarinet.dsp";filename="clarinet.dsp"'  http://localhost:8888/compile/android/android/binary.apk --output binary3.apk

#### Google Cloud compilation

	curl http://35.229.105.70/faustservice/targets
	curl -F'file=@"clarinet.dsp";filename="clarinet.dsp"' http://35.229.105.70/faustservice/compile/android/android/binary.apk --output binary3.apk
	curl http://35.229.105.70/faustservice/5ADBDAF2AFFF8387F4FCB9F05BA84E374DE3ABAF/android/android/binary.apk --output binary3.apk
	journalctl -f /home/orlarey/FaustInstall/faustservice/faustweb
	sudo systemctl restart faustweb
	sudo systemctl restart apache2

#### Playing with faustservice on Google VM

	curl http://35.190.180.34/targets
	curl http://104.196.34.150/targets

#### Testing locally

	curl localhost:8888/targets
	curl -F'file=@"kisana.dsp";filename="kisana.dsp"' localhost:8888/filepost
	curl -F'file=@"kisana.dsp";filename="kisana.dsp"' localhost:8888/compile/android/android/binary.zip --output kisana.apk

#### Using the remote service

The remote compilation service run on the http://faustservice.grame.fr URL, and can be used like in the following examples:

    curl http://faustservice.grame.fr/targets
    curl -F'file=@"kisana.dsp";filename="kisana.dsp"' http://faustservice.grame.fr/filepost
    curl -F'file=@"kisana.dsp";filename="kisana.dsp"' http://faustservice.grame.fr/compile/soul/soul/binary.zip --output kisana.zip


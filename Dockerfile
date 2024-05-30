
########################################################################
########################################################################
#
#       Faustservice (remote Faust compiler) in a docker
#                 (L. Champenois & Y. Orlarey)
#
########################################################################
########################################################################

FROM grame/faustready-ubuntu-1604:004

########################################################################
# Now we can clone and compile all the Faust related git repositories
########################################################################

RUN echo "CHANGE THIS NUMBER TO FORCE REGENERATION : 013"

#
# Makeself is a utility for building self-extracting/installing archives that
# need no extra support on the machine on which they are run. This is very useful
# for building installers that don't really need to be unzipped before they are
# executed - ala the Chaos Audio Stratus pedal installer.
#
RUN apt install makeself

# faustservice first as it changes less often
RUN git clone -n https://github.com/grame-cncm/faustservice.git; \
    git -C faustservice checkout server; \
    make -C faustservice

# then the faust compiler itself (fast cloning from DF)
RUN git clone --single-branch --recurse-submodules --depth 1 https://github.com/grame-cncm/faust.git; \
    make -C faust; \
    make -C faust install


########################################################################
# Tune image by forcing Gradle upgrade
########################################################################
ENV GRADLE_USER_HOME=/tmp/gradle

RUN echo "process=+;" > tmp.dsp; \
    faust2android tmp.dsp; \
    faust2smartkeyb -android tmp.dsp; \
    rm tmp.apk


########################################################################
# And starts Faustservice
########################################################################
EXPOSE 80
WORKDIR /faustservice
RUN cp ./bin/dockerOSX /usr/local/bin/; \ 
    rm -rf makefiles/osx; \
    mv makefiles/dockerosx makefiles/osx; \
    rm -rf makefiles/windows64 makefiles/ros makefiles/unity/all makefiles/unity/osx

CMD ./faustweb --port 80 --sessions-dir /tmp/sessions



########################################################################
# HowTo run this docker image
########################################################################
# For local tests:
#-----------------
# docker run -it -v /var/run/docker.sock:/var/run/docker.sock -v /tmp/sharedfaustfolder:/tmp/sharedfaustfolder -p 80:80 grame/faustservice:latest
#
# For production:
#----------------
# docker run -d --restart=always -v /var/run/docker.sock:/var/run/docker.sock -v /tmp/sharedfaustfolder:/tmp/sharedfaustfolder -p 80:80 grame/faustservice:latest
#
# Old production:
#----------------
# docker run -d --restart=always -v /var/run/docker.sock:/var/run/docker.sock -v /tmp/sharedfaustfolder:/tmp/sharedfaustfolder -p 80:80 grame/faustservice-ubuntu-1604-neuf-tuned:latest

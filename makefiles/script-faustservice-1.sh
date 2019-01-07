#! /bin/bash
docker run -v /var/run/docker.sock:/var/run/docker.sock -v /tmp/sharedfaustfolder:/tmp/sharedfaustfolder -p 80:80 grame/faustservice-ubuntu-1604-five-tuned:latest
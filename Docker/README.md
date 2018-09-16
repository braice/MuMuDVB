# About MuMuDVB as a Docker container
Following the request for an [official Docker container](https://github.com/braice/MuMuDVB/issues/197), this is a proposal how to build a Docker image for [MuMuDVB](http://mumudvb.net).

Advantages of running MuMuDVB as a container include:
- Simplified runtime, you don't need a pre-compiled package for your specific Linux distro and version.
- Simplified building, all that is required to build MuMuDVB will be included into the build-container, you don't need to install gcc on your host Linux.

Things to know when running MuMuDVB as a container include:
- Slight longer startup and shutdown times as usual with containers.
- Host Linux must support the DVB Hardware (e.g. have the driver), the container uses the hosts kernel dvb-api (--device).
- When thinking about clustering and Kubernetes and such: you need a _/dev/dvb/_ to map into the container on the executing node.

Disadvantages when running MuMuDVB inside a container include:
- Docker might not support UDP well, please report your experiences with your multi- and broadcast scenarios. 
- http playlist/m3u generation fails as the m3u-streams are addressed with the docker internal ip. However, accessing the streams _bysid/123_ url works fine.

# About this proposal
Looking at how to wrap-up MuMuDVB into an image, there's a number of parameters to be considered. MuMuDVB compiles with more features (such as [CAM](https://en.wikipedia.org/wiki/Conditional-access_module)-support) when the required libraries are found (libdvbcsa in this case), and personally I usually like to have some additional debugging tools inside the Image as well.

I ended up with tree variants: One simple just MuMuDVB without any further features, one with CAM/SCAM enabled, and one with 3rd tools like _w_scan_ or _dvblast_.

See the [Dockerfile](Dockerfile): there are sections commented-out with _`#pattern;`_. In order to build a variant with _cam_ or _tool_ enabled, we simply remove these patterns before build with `sed`. [This is not necessarily a nice way](https://stackoverflow.com/questions/52041227/how-to-deal-with-multiple-variants-of-application-using-same-dockerfile).

# Build 
## build _simple_ (no features, no tools)
```bash
docker build -t mumudvb:simple . 
```
## build _cam_ (S/CAM support, no tools)
```bash
sed -r 's_^#(cam|scam);__g' Dockerfile | docker build -t mumudvb:cam . -f -
```
## build _sak_ (SwissArmyKnife, includes CAM and tools)
```bash
sed -r 's_^#(cam|scam|tool);__g' Dockerfile | docker build -t mumudvb:sak . -f -
```
## verify the builds
```bash
# list images
docker images | grep ^mumudvb
# no CAM support
docker run --rm -it mumudvb:simple mumudvb | grep CAM
# with CAM support
docker run --rm -it mumudvb:cam mumudvb | grep CAM
# with dvblast installed as well
docker run --rm -it mumudvb:sak dvblast -V
```

# Run
## explore and compare the container(s)
```bash
docker run -it --rm   mumudvb:simple  /bin/bash
docker run -it --rm   mumudvb:sak     /bin/bash
``` 

## run a scan
```bash
docker run -it --rm --device /dev/dvb/    mumudvb:sak    w_scan -f t -a /dev/dvb/adapter0/
``` 
>
*NOTE*
- host device-tree _/dev/dvb_ is passed into the container.
- Mind Docker's stdOut and -Err behaviour when capturing the w_scan out.
- Image to be run is the _sak_
> 

## run mumudvb
adjust the _sample.conf_ to your needs, and run mumudvb inside the container
```bash
docker run -it --rm  --name my_mumu_adapter0 \
     --device /dev/dvb/adapter0 \
     --publish 4242:4242 \
     --volume ${PWD}/sample.conf:/mumudvb.conf \
     mumudvb:simple \
     mumudvb -d -c /mumudvb.conf
```
Upon mumudvb startup, try to access [http://host-ip:4212](http://127.0.0.1:4212).
> 
*NOTE*
- If you get a _permission denied_, the configfile could not be accessed from Docker-Daemon. Check your working-directroy for read-permission from group docker.
- _--name_: give this container instance a specific name.
- _--device_: map only one adapter into this container.
- _--publish_: map external/host TCP/4212 into Containers TCP/4212.
- _--volume_: map the _sample.conf_ in current directory to _/mumudvb.conf_ in the container.
- Image to be run is the _simple_
>

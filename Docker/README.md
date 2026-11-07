# About MuMuDVB as a Docker container
Following the request for an [official Docker container](https://github.com/braice/MuMuDVB/issues/197), this is a proposal how to build a Docker image for [MuMuDVB](http://mumudvb.net).

Advantages of running MuMuDVB as a container include:
- Simplified runtime, you don't need a pre-compiled package for your specific Linux distro and version.
- Simplified building, all that is required to build MuMuDVB will be included into the build-container, you don't need to install gcc on your host Linux.

Things to know when running MuMuDVB as a container include:
- Slightly longer startup and shutdown times as usual with containers.
- Host Linux must support the DVB Hardware (e.g. have the driver), the container uses the host kernel DVB API (`--device`).
- When thinking about clustering and Kubernetes and such: you need a `/dev/dvb/` to map into the container on the executing node.

Disadvantages when running MuMuDVB inside a container include:
- Docker multicast/UDP networking often needs host networking or careful routing; see notes below.
- HTTP playlist/m3u generation can advertise the container-internal IP. Accessing streams via `/bysid/<sid>` usually works fine.

# About this proposal
Looking at how to wrap MuMuDVB into an image, there are a number of parameters to be considered. MuMuDVB compiles with more features when the required libraries are found:
- **Hardware CAM** support needs libdvben50221 / libucsi (linuxtv dvb-apps)
- **Software descrambling (SCAM)** needs libdvbcsa

Three variants are provided: one simple MuMuDVB without optional features, one with CAM/SCAM enabled, and one with extra tools like `w_scan` or `dvblast`.

See the [Dockerfile](Dockerfile): there are sections commented-out with `#pattern;`. In order to build a variant with `cam` or `tool` enabled, we simply remove these patterns before build with `sed`.

[NOTE]
The current Dockerfile still targets Fedora 28 as a historical base. Treat it as a starting point; prefer a current Fedora/Debian base when rebuilding for production.

# Build
## build _simple_ (no features, no tools)
```bash
docker build -t mumudvb:simple .
```
## build _cam_ (CAM/SCAM support, no tools)
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
- host device-tree `/dev/dvb` is passed into the container.
- Mind Docker's stdout/stderr behaviour when capturing the w_scan output.
- Image to be run is the _sak_
>

## run mumudvb (HTTP unicast)
`sample.conf` sets `port_http=4242+%card*10+%tuner*1`, so adapter0/tuner0 listens on **TCP 4242**.

```bash
docker run -it --rm  --name my_mumu_adapter0 \
     --device /dev/dvb/adapter0 \
     --publish 4242:4242 \
     --volume ${PWD}/sample.conf:/mumudvb.conf \
     mumudvb:simple \
     mumudvb -d -c /mumudvb.conf
```
Upon mumudvb startup, try to access [http://127.0.0.1:4242](http://127.0.0.1:4242).
>
*NOTE*
- If you get a _permission denied_, the config file could not be accessed from the Docker daemon. Check that the working directory is readable by the docker group.
- `--name`: give this container instance a specific name.
- `--device`: map only one adapter into this container.
- `--publish`: map host TCP 4242 to container TCP 4242 (match `port_http` in your config).
- `--volume`: map `sample.conf` in the current directory to `/mumudvb.conf` in the container.
- Image to be run is the _simple_
>

## Multicast note
For multicast output, prefer host networking so IGMP/multicast routing works as on a normal host:

```bash
docker run -it --rm --name my_mumu_adapter0 \
     --network host \
     --device /dev/dvb/adapter0 \
     --volume ${PWD}/sample.sat.conf:/mumudvb.conf \
     mumudvb:simple \
     mumudvb -d -c /mumudvb.conf
```

With `--network host`, `--publish` is unnecessary; clients use the host IP and the ports from your config.

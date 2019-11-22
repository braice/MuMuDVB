# Host with DigitalDevices Adapter
Install ddbridge-driver for Digital Devices [Cine S2 V6.5](https://digitaldevices.de/products/dvb_components/cine_s2) and [DuoFlex CI](https://digitaldevices.de/products/dvb_components/duoflex_ci) on redhat flavour.

## install ddbridge driver to host
```
dnf install -y kernel-headers-`uname -r` kernel-devel-`uname -r` elfutils-libelf-devel git gcc gcc-c++ make usbutils
 
rmmod ddbridge dvb_core cxd2099
cd /usr/local/src
git clone https://github.com/DigitalDevices/dddvb.git
pushd dddvb
make && make install
echo "search extra updates built-in" > /etc/depmod.d/extra.conf
echo "options ddbridge adapter_alloc=3" > /lib/modprobe.d/ddbridge.conf
echo "options dvb_core debug=0 cam_debug=1" >>  /lib/modprobe.d/ddbridge.conf
depmod -a
modprobe ddbridge
dmesg
popd
# reboot?
 
# about TAB/CAM:  https://www.spinics.net/lists/linux-media/msg39494.html     http://mumudvb.net/documentation/asciidoc/mumudvb-2.0.0/README.html#_hardware_cam_issues
# wire: Tuner0 -> Input0 -> Port0 (TAB1, DVB-S2) ==>  Port2(TAB3, CAM1)
# wire: Tuner0 -> Input1 -> Port0 (TAB1, DVB-S2) ==>  Port3(TAB4, CAM2)
 
# reload the driver before re-wireing the TABs
sudo rmmod ddbridge dvb_core cxd2099; sudo modprobe ddbridge
echo "00 02" | sudo tee /sys/class/ddbridge/ddbridge0/redirect
echo "01 03" | sudo tee /sys/class/ddbridge/ddbridge0/redirect

# you might add the above to /etc/rc.d/rc.local 
```

## install docker
Assuming Fedora(28)
```
# remove previous installation (if any)
sudo dnf remove docker docker-client docker-client-latest docker-common docker-latest docker-latest-logrotate docker-logrotate docker-selinux docker-engine-selinux docker-engine

# following https://docs.docker.com/install/linux/docker-ce/fedora/#install-using-the-repository
sudo dnf -y install dnf-plugins-core
sudo dnf config-manager --add-repo https://download.docker.com/linux/fedora/docker-ce.repo
sudo dnf install docker-ce
sudo systemctl enable docker; sudo systemctl restart docker
sudo usermod -a -G docker `whoami`
```


# a sample configuration
```
Monoblock LNB, Astra(DiSEqC 0) & Hotbird (DiSEqC 1)
   | | | |  
   | | | |    
   | | | |  
 +---------+
 |         |
 | EXR2908 |   (DiSEqC Switch)
 |         |
 +---------+
   |     | 
   |     | 
   |     |   |
-------------+
  |         |
  | C     |||  TAB1 -> CI TAB1 with TechniSat CAM and SRF Access Card
  | i     |||
--+ n       |
|   e     |||  TAB2 -> CI TAB2 with TechniSat CAM and SRF Access Card
|         |||
|   S       |
--+ 6     |||  TAB3 ->  empty
  | .     |||
  | 5       |
  +---------+
```

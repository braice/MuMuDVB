MuMuDVB - README
================
Brice Dubost <mumudvb@braice.net>
Version 1.6.1

[NOTE]
An HTML version of this file is availaible on http://mumudvb.braice.net[MuMuDVB's website].

image::http://mumudvb.braice.net/mumudvb/logo_mumu_wiki.png[caption="logo MuMuDVB"]

Presentation
------------

Description
~~~~~~~~~~~

MuMuDVB (Multi Multicast DVB) is originally a modification of dvbstream that http://www.crans.org[cr@ns] made. We have decided to redistribute it.

Now, it's a standalone project.

MuMuDVB is a program that redistributes streams from DVB (Digital Television) on a network using
multicasting or HTTP unicast. It can multicast a whole DVB transponder by assigning
each channel a different multicast IP.

Website
~~~~~~~

http://mumudvb.braice.net[Mumudvb main site]


Authors and contacts
--------------------

.Upstream author
- mailto:mumudvb@braice.net[Brice Dubost]

.Contributions
- mailto:manu@REMOVEMEcrans.ens-cachan.fr[Manuel Sabban] (getopt)
- mailto:glondu@REMOVEMEcrans.ens-cachan.fr[Stéphane Glondu] (makefile cleaning, man page, debian package)
- Special thanks to Dave Chapman (dvbstream author)
- Pierre Gronlier, Sébastien Raillard


.Mailing list:
- mailto:mumudvb-dev@REMOVEMElists.crans.org[MuMuDVB dev]
- https://lists.crans.org/listinfo/mumudvb-dev[MuMuDVB dev list information and subscription]

[NOTE]
When contacting about an issue, please join the config file used and the output of MuMuDVB in verbose mode ( -vvv on the command line) and any other information that could be useful.


Contents and features
---------------------

Features overview
~~~~~~~~~~~~~~~~~

- Stream channels from a transponder on different multicast IPs
- Support for scrambled channels (if you don't have a CAM you can use sasc-ng, but check if it's allowed in you country/by your broadcaster)
- Support for automatic configuration i.e channels discovery, see <<autoconfiguration,Autoconfiguration>> section
- Generation of SAP announces, see <<sap,SAP>> section
- Support of DVB-S2, DVB-S, DVB-C, DVB-T and ATSC
- Possibility to partially rewrite the stream for better compatibility with set-top boxes and some clients. See <<pat_rewrite,PAT Rewrite>> and <<sdt_rewrite,SDT Rewrite>> sections.
- Support for HTTP unicast see <<unicast,http unicast>> section
- Support for RTP headers (only for multicast)

Detailled feature list
~~~~~~~~~~~~~~~~~~~~~~

- Can show reception level when streaming
- Show if channels are successfully descrambled by the CAM
- Makes a list of streamed and down channels in real time
- Can deamonize and write his own process id in a file
- Supports standard and universals LNBs
- Can stop himself if it receives no data from the card after a defined timeout
- Stops trying tuning after a configurable timeout
- The mandatory pids are always sent with all channels : 
  * PAT (0): Program Association Table
  * CAT (1): Conditionnal Access Table
  * NIT (16): Network Information Table: intended to provide information about the physical network.
  * SDT (17): Service Description Table: data describing the services in the system e.g. names of services, the service provider, etc.
  * EIT (18): Event Information Table: data concerning events or programmes such as event name, start time, duration, etc.
  * TDT (20): Time and Date Table: information related to the present time and date.This information is given in a separate table due to the frequent updating of this information.
- Can suscribe to all multicast groups (IGMP membership request) in order to avoid some switches to broadcast all channels see <<problems_hp,problems with HP switches>>.
- Supports autoconfiguration, see <<autoconfiguration,Autoconfiguration>> section
  * In autoconfiguration mode, MuMuDVB follow the changes in the PIDs and update itself while running.
- Debian flavor initialisation scripts
- The buffer size can be tuned to reduce CPU usage, see <<reduce_cpu,reduce MuMuDVB CPU usage>> section.
- Can avoid the sending of scrambled packets
- Automatically detect the scrambling status of a channel
- Can reset the CAM module in case of a bad initialisation
- Can sort the EIT PID to send only the ones corresponding to the current channel
- Data reading can be done using a thread, see <<threaded_read, thread reading>> section.

Others small programs are availaible from http://gitweb.braice.net/gitweb?p=mumudvb_tools;a=summary[MuMuDVB Tools Repository] :

- relay: this program takes a stream and sends it again on a different ip. It can be used to unicast a stream over a network that doesn't support multicast, and multicast it again at the other end
- recup_sap: tiny sap client, get sap announces during 60 seconds and put them on a file (unsorted)


Installation
------------

From sources
~~~~~~~~~~~~

From a snapshot
^^^^^^^^^^^^^^^

If you downloaded a snapshot, you will have to generate the auto(conf make etc ...) files. In order to do this you will need the autotools, automake and libtool and, type in the folder of MuMuDVB

----------------
autoreconf -i -f
----------------

Then you have a source which can be installed as a release package.

From a release package
^^^^^^^^^^^^^^^^^^^^^^

In order to install MuMuDVB type:

---------------------------------
$ ./configure [configure options]
$ make
# make install
---------------------------------

The `[configure options]` specific to MuMuDVB are:

---------------------------------------------------------------------
  --enable-cam-support    CAM support (default enabled)
  --enable-coverage       build for test coverage (default disabled)
  --enable-duma           Debbuging DUMA library (default disabled)
---------------------------------------------------------------------

You can have a list of all the configure options by typing

--------------------
$ ./configure --help
--------------------

[NOTE]
The CAM support depends on libdvben50221, libucsi (from linuxtv's dvb-apps) and libpthread. The configure script will detect automatically the presence of these libraries and desactivate the CAM support if one of them is not present.

[NOTE]
The decoding of long channel names for autoconfiguration in ATSC depends on libucsi (from linuxtv's dvb-apps). The configure script will detect automatically the presence of this library and desactivate the long channel name support if it is not present. The full autoconfiguration will still work with ATSC but the channel names will be the short channels names (7 characters maximum)

[NOTE]
If you want to compile the doc i.e. generate HTML files using asciidoc, type `make doc`. The rendering for the tables will work with asciidoc 8.4.4 (can work with lower version but not tested).

In order to install starting scripts (debian flavor) type:

------------------------------------------------------------
# cp scripts/debian/etc/default/mumudvb /etc/default/mumudvb
# cp scripts/debian/etc/init.d/mumudvb /etc/init.d/mumudvb
------------------------------------------------------------

[NOTE]
It is advised to create a system user for MuMuDVB, e.g. : `_mumudvb`, you have to add this user to the video group and make the directory `/var/run/mumudvb` RW by this user. By doing this, you'll be able to get all the features of MuMuDVB.


From Debian package
~~~~~~~~~~~~~~~~~~~

If you want to install a version which is not in your repositories, you can install it by hand by typing:

----------------------
# dpkg -i mumudvb*.deb
----------------------

Otherwise you can use aptitude/synaptic as usual

Usage
-----

The documentation for configuration file syntax is in `doc/README_CONF.txt` (link:README_CONF.html[HTML version]).

Usage:

--------------------------------------
mumudvb [options] -c config_file
mumudvb [options] --config config_file
--------------------------------------

Possible options are:

------------------------------------------------------------------
-d, --debug
	Don't deamonize and print messages on the standard output.

-s, --signal
	Print signal strenght every 5 seconds

-t, --traffic
	Print the traffic of the channels every 10 seconds

-l, --list-cards
	List the DVB cards and exit

-h, --help
	Show help

-v
	More verbose (add for more)

-q
	More quiet (add for less)
------------------------------------------------------------------

Signal: (see kill(1))
------------------------------------------------------------------
    SIGUSR1: switch the signal strenght printing
    SIGUSR2: switch the traffic printing
------------------------------------------------------------------

[[autoconfiguration]]
Autoconfiguration
-----------------

MuMuDVB is able to find the channels in the transponder and their PIDs (Program IDentifiers).

Without autoconfiguration, you have to set the transponder parameters, and for each channel, the multicast ip, the name and the PIDs (PMT, audio, video, teletext etc...)

At the end of autoconfiguration, MuMuDVB generates a config file with the discovered parameters. This file is: `/var/run/mumudvb/mumudvb_generated_conf_card%d`

If the PIDs are changed, MuMuDVB will automatically update the channels except if you put `autoconf_pid_update=0` in your configuration file.

MuMuDVB is able to do two kinds of autoconfiguration:

Full autoconfiguration
~~~~~~~~~~~~~~~~~~~~~~

This is the easiest way to use MuMuDVB.

Use this when you want to stream a full transponder.

In this mode, MuMuDVB will find for you the different channels, their name and their PIDs (PMT, PCR, Audio, Video, Subtitle, Teletext and AC3).

In order to use this mode you have to:
- Set the tuning parameters to your config file
- Add `autoconfiguration=2` to your config file
- You don't have to set any channels
- For a first use don't forget to put the `-d` parameter when you launch MuMuDVB:
   e.g. `mumudvb -d -c your_config_file`

.Example config file for satellite at frequency 11.296GHz with horizontal polarization
--------------------
freq=11296
pol=h
srate=27500
autoconfiguration=2
--------------------

The channels will be streamed over the multicasts ip adresses 239.100.c.n where c is the card number (0 by default) and n is the channel number.

If you don't use the common_port directive, MuMuDVB will use the port 1234.

[NOTE]
By default, SAP announces are activated if you use this autoconfiguration mode. To desactivate them put `sap=0` in your config file.
By default, SDT rewriting is activated if you use this autoconfiguration mode. To desactivate them put `rewrite_sdt=0` in your config file.
By default, PAT rewriting is activated if you use this autoconfiguration mode. To desactivate them put `rewrite_pat=0` in your config file.
By default, EIT sorting activated if you use this autoconfiguration mode. To desactivate them put `sort_eit=0` in your config file.

[NOTE]
A detailled, documented example configuration file can be found in `doc/configuration_examples/autoconf_full.conf`

[[autoconfiguration_simple]]
Simple autoconfiguration
~~~~~~~~~~~~~~~~~~~~~~~~

Use this when you want to control the name of the channels, and wich channel you want to stream.


- You have to add 'autoconfiguration=1' in the head of your config file.
- For each channel, you have to set:
 * the Ip adress
 * the name
 * the PMT PID


MuMuDVB will find the audio, video, PCR, teletext, subtitling and AC3 PIDs for you before streaming.

[NOTE]
If you put more than one PID for a channel, MuMuDVB will desactivate autoconfiguration for this channel.

[NOTE]
A detailled, documented example configuration file can be found in `doc/configuration_examples/autoconf1.conf`

[NOTE]
Simple autoconfiguration can fail finding the good pids if a PMT pid is shared within multiples channels. In this case you have to add the `ts_id` option to the channel to specify the transport stream id, also known as service id.

[[sap]]
SAP announces
-------------

SAP (Session Announcement Protocol) announces are made for the client to know which channels are streamed and what is their name and adress. It avoids to give to the client the list of the multicast ip adresses.

VLC and most of set-top boxes are known to support them.

MuMuDVB will automatically generate and send SAP announces if asked to in the config file or if you are in full autoconfiguration mode.

The SAP announces will be only sent for alive channels. When a channel goes down, MuMuDVB will stop sending announces for this channel, until it goes back.


Asking MuMuDVB to generate SAP announces
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For sending SAP announces you have to add `sap=1` to your config file. The other parameters concerning the sap announces are documented in the `doc/README_CONF.txt` file (link:README_CONF.html[HTML version]).

Configuring the client to get the SAP announces
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

VLC > 0.8.2
^^^^^^^^^^^
You have to enter the settings, choose advanced settings. The SAP announces are in playlist->service discovery.

Don't forget to save the settings.

You should have now a SAP section in your playlist.

VLC < 0.8.2
^^^^^^^^^^^

Click on the "Settings" menu, then on "add interface" and choose SAP playlist. Then open you playlist, the SAP announces should appear automatically.



[[unicast]]
HTTP Unicast
------------

In addition to multicast, MuMuDVB also supports HTTP unicast. This make you able to use MuMuDVB on networks wich doesn't support multicast.

There is one listening connection, the channel is selected via the HTTP path, see further.

And you can have listening sockets per channel, in this case the client will always get the same channel independantly of the path.

[NOTE]
Be careful with unicast, it can eat a lot of bandwith. Think about limitting the number of clients.

[NOTE]
If you don't want the (always here) multicast traffic to go on your network set `multicast=0`

Activate HTTP unicast
~~~~~~~~~~~~~~~~~~~~~

To enable HTTP unicast you have to set the option `unicast`. By default MuMuDVB will listen on all your interfaces for incoming connections.

You can also define the listening port using `port_http`. If the port is not defined, the default port will be 4242.

Activate "per channel" listening socket
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can create listening connections only for a channel. In this case, when a client connect to this socket he will alway get the same channel independantly of the HTTP path.

If you use full autoconfiguration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You need to set the option `autoconf_unicast_start_port` which define what is the output port for the first discovered channel (for the following channels the port will be incremented).


If you don't use full autoconfiguration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For the channels for which you want to have a listening unicast socket you have to set the option `unicast_port` which define the listening port of the socket



Client side, the different methods to get channels
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Single channel socket
^^^^^^^^^^^^^^^^^^^^^

If the client connect to a single client socket he will get the associated channel independantly of the path.

If you server is listening on the ip 10.0.0.1 and the port for the channel is 5000,

-------------------------
vlc http://10.0.0.1:5000/
-------------------------

Get the channel by number
^^^^^^^^^^^^^^^^^^^^^^^^^

You can ask the channel by the channel number (starting at 1).

If you server is listening on the ip 10.0.0.1 and the port 4242,

------------------------------------
vlc http://10.0.0.1:4242/bynumber/3
------------------------------------

will give you the channel number 3. This works also with xine and mplayer.

Get the channel by name
^^^^^^^^^^^^^^^^^^^^^^^

[NOTE]
This is not implemented for the moment, it will be implemented in a future release

Get the channels list
^^^^^^^^^^^^^^^^^^^^^

If you server is listening on the ip 10.0.1 and the port 4242,

To get the channel list (in basic html) just enter the adress `http://10.0.0.1:4242/channels_list.html` in your web browser.

To get the channel list (in basic plain text with more informations) just enter the adress `http://10.0.0.1:4242/channels_list.txt` in your web browser.

HTTP unicast and monitoring
~~~~~~~~~~~~~~~~~~~~~~~~~~~

This HTTP connection can be used to monitor MuMuDVB. This feature will be implemented in a near future release. Please tell wich data can be interesting for you.

You can get statistics using the following url `http://10.0.0.1:4242/monitor/statistics.txt`


Monitoring
----------

You can use http://mmonit.com/monit/[Monit] to monitor MuMuDVB an restart it when it experiences problems (MuMuDVB kill himself when big issues appear).

You have to install the init scripts (automatic if you used the Debian package) and add the following lines to your `/etc/monit/services` file:

----------------------------------------------------------------------
check process mumudvb with pidfile /var/run/mumudvb/mumudvb_carte0.pid
    start program = "/etc/init.d/mumudvb start"
    stop program = "/etc/init.d/mumudvb stop"
----------------------------------------------------------------------

[NOTE]
The 0 have to be replaced by the DVB card number if you have multiples cards.

For more detailled information, refer to the http://mmonit.com/monit/[Monit Website].

MuMuDVB usually run for many days without problems, but with monit you are safe. Monit is also able to send e-mails in case of problems.


Scrambled channels support
--------------------------

Important note : check the contract with your broadcaster to see if you are allowed to stream the scrambled channels you're subscribed to.

Hardware descrambling
~~~~~~~~~~~~~~~~~~~~~

MuMuDVB supports scrambled channels via hardware descrambling i.e. a CAM (Conditionnal Access Module). It can ask the CAM to descramble multiple channels if the CAM supports it (Aston Pro, or PowerCam Pro are known to work with multiple channels).

If you are limited by the number of PIDs the can can decrypt simultaneously, it is possible to ask the CAM to decrypt only the audio and video. This feature is not implemented, please ask if you need it.

[NOTE]
The hardware descramblig uses almost no CPU, all the descrambling is made by the CAM.

[NOTE]
MuMuDVB doesn't query the CAM before asking for descrambling. The query is not reliable. Most of CAMs answer a menu when the descrambling is not possible and MuMuDVB will display it on the standard error.

The information concerning the CAM is stored in '''/var/run/mumudvb/caminfo_carte%d''' where %d is the DVB card number.

.Example contents of '''/var/run/mumudvb/caminfo_carte%d''' 
----------------------------------------------------
CAM_Application_Type=01
CAM_Application_Manufacturer=02ca
CAM_Manufacturer_Code=3000
CAM_Menu_String=PowerCam_HD V2.0
ID_CA_Supported=0100
ID_CA_Supported=0500
----------------------------------------------------


How to ask MuMuDVB for descrambling?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.You are using full autoconfiguration :

Just add `cam_support=1` to your config file

.You are using partial autoconfiguration or no autoconfiguration
 * Add `cam_support=1` to your config file (before the channels)
 * For each scrambled channel add the `cam_pmt_pid` option. This option is made for MuMuDVB to know wich PID is the PMT PID wich will be used to ask for descrambling

[NOTE]
You have an example of CAM support in doc/configuration_examples/autoconf1.conf



Software descrambling
~~~~~~~~~~~~~~~~~~~~~

Important note : this solution is not allowed by some provider contracts.

MuMuDVB has been reported to work with software descrambling solutions like sascng + newcs + dvbloopback.

In this case  you don't need to set the `cam_support` option. Just ajust the `card` option to fit with your virtual dvbloopback card. 

If you use these solutions, see <<reduce_cpu,reduce MuMuDVB CPU usage>> section.

Scrambling status
~~~~~~~~~~~~~~~~~

The scrambling status is stored together with the streamed channel list. 

.Example
----------------------------------------------
239.100.0.7:1234:ESCALES:PartiallyUnscrambled
239.100.0.8:1234:Fit/Toute l'Histoire:PartiallyUnscrambled
239.100.0.9:1234:NT1:PartiallyUnscrambled
239.100.0.10:1234:ACTION:PartiallyUnscrambled
239.100.0.11:1234:MANGAS:PartiallyUnscrambled
239.100.0.12:1234:ENCYCLOPEDIA:PartiallyUnscrambled
239.100.0.13:1234:XXL PL:PartiallyUnscrambled
239.100.0.14:1234:France 5:HighlyScrambled
239.100.0.16:1234:LCP:FullyUnscrambled
239.100.0.17:1234:VIDEOCLICK:FullyUnscrambled
----------------------------------------------

 * FullyUnscrambled : less than 5% of scrambled packets
 * PartiallyUnscrambled : between 5% and 95% of scrambled packets
 * HighlyScrambled : more than 95% of scrambled packets

[[pat_rewrite]]
PAT (Program Allocation Table) Rewriting
-----------------------------------------

This feature is mainly intended for set-top boxes. This option will announce only the streamed channel in the Program Allocation Table instead of all transponder channels. Computer clients parse this table and decode the first working program. Set-top boxes usually try only the first one which give usually a blank screen in most of the channels. 

To enable PAT rewriting, add `rewrite_pat=1` to your config file. This feature consumes few CPU, since the rewritten PAT is stored in memory and computed only once per channel.

[NOTE]
PAT rewrite can fail (i.e. doesn't solve the previous symptoms) for some channels if their PMT pid is shared. In this case you have to add the `ts_id` option to the channel to specify the transport stream id, also known as service id.

[[sdt_rewrite]]
SDT (Service Description Table) Rewriting
-----------------------------------------

This option will announce only the streamed channel in the Service Description Table instead of all transponder channels. Some clients parse this table and can show/select ghost programs if it is not rewritten (even if the PAT is). This can rise to a random black screen.

To enable SDT rewriting, add `rewrite_sdt=1` to your config file. This feature consumes few CPU, since the rewritten SDT is stored in memory and computed only once per channel.

[NOTE]
If you don't use full autoconfiguration, SDT rewrite needs the `ts_id` option for each channel to specify the transport stream id, also known as service id.



EIT PID (Event Information Table) Sorting
-----------------------------------------

This option will make MuMuDVB stream only the EIT packets corresponding to the streamed channel instead of all transponder channels. Some clients parse this table and can show/select ghost programs  (even if the PAT and the SDT are rewritten).

To enable EIT sorting, add `sort_eit=1` to your config file. 

[NOTE]
If you don't use full autoconfiguration, EIT sorting needs the `ts_id` option for each channel to specify the transport stream id, also known as service id.

[[reduce_cpu]]
Reduce MuMuDVB CPU usage
------------------------

Normally MuMuDVB reads the packets from the card one by one and ask the card if there is data avalaible between each packets (poll). But often the cards have an internal buffer. Because of this buffer, some pollings are useless. These pollings eat some CPU time.

To reduce CPU usage, one solution is to try to read several packets at the same time. To do this use the option `dvr_buffer_size`.

.Example
------------------
dvr_buffer_size=40
------------------

To see if the value you put is too big or to low, run MuMuDVB in verbose mode, the average number of packets received at the same time will be shown every 2 minutes. If this number if below your buffer size, it is useless to increase it. 

The CPU usage reduction can be between 20% and 50%.

[[threaded_read]]
Data reading using a thread
---------------------------

In order to make MuMuDVB more robust (at the cost of a slight CPU consumption increase), MuMuDVB can read the data from the card using a thread. This make the data reading "independant" of the rest of the program.

In order to enable this feature, use the option `dvr_thread`.

This reading uses two buffers: one for the data just received from the card, one for the data treated by the main program. You can adjust the size of this buffers using the option `dvr_thread_buffer_size`. The default value  (5000 packets of 188 bytes) should be sufficient for most of the cases. 

The message "Thread trowing dvb packets" informs you that the thread buffer is full and some packets are dropped. Increase the buffer size will probably solve the problem.


Technical details (not sorted)
------------------------------

 * CPU consuming: MuMuDVB takes 15% CPU of a celeron 2.6GHz with an Hauppauge card and linux version 2.6.9 when streaming a full transponder (about 30MBit/s)

 * Try to avoid old via or nForce chipsets and in general ultra low cost motherboards. They can't deal with a lot of data on the PCI bus.

 * When the program starts, he writes the channel list in the file `/var/run/mumudvb/chaines_diffusees_carte%d` (where %d is the card number). This file contains streamed channels (updated every 5 seconds) in the form: "name:ip:port:Scramblingstatus"

 * MuMuDVB is able to support as many cards as the operating system does. Old versions of udev+glibc were not able to support more than 4 cards but this problem is solved using relatively recent versions (udev > 104 and libc6 > 2.7)

 * When daemonized, MuMuDVB writes its process identifier in `/var/run/mumudvb/mumudvb_carte%d.pid`, where %d is replaced by the card number

 * MuMuDVB supports satellite in the Ku band, with universal or standard LNBs. The support of satellites in the S or C band is implemented via the use of the lo_frequency option. See `doc/README_CONF.txt` (link:README_CONF.html[HTML version]).

Known issues
------------

VLC can't read the stream but it is fine with xine or mplayer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 * For VLC, you must specify the PMT PID besides audio and video PIDs.
It's a frequent issue. To solve it you can use the verbose mode of VLC (`vlc -v`) and you'll see a ligne like: `[00000269] ts demuxer debug:   * number=1025 pid=110` you'll have the PMT PID associated with your program number, you can also use dvbsnoop, or see how to get pids in `doc/README_CONF.txt` (link:README_CONF.html[HTML version]). Another solution is to use full autoconfiguration.

VLC reads the video but no audio
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 * This problem can happend if the PCR (i.e. clock) information is not carried with the video. In this case you have to check if the PCR PID is in the PIDs list.

MuMuDVB can't deamonize
~~~~~~~~~~~~~~~~~~~~~~~

 * In order to deamonize, MuMuDVB needs the directory `/var/run/mumudvb/` to be writable, in order to write his process identifier and the channel list.

The system crashes or freeze
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 * Old via chipset or nForce chipset are not professional chipsets. They can't deal with a lot of data on PCI. But you can try to tune your BIOS.

Tuning issues with DVB-T
~~~~~~~~~~~~~~~~~~~~~~~~

 * You must check tuning settings, knowing that auto bandwidth usually does'nt work.

The set-top box display a blank screen
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 * If the stream is working well when reading it with a computer and not with your set-top box, this is probably because your set-top box needs the PAT PID to be rewritten. To do this add `rewrite_pat=1` to your config file.

Simple autoconfiguration fails finding the right pids
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 * This can happend if a PMT PIDs is shared among multiple channels, see <<autoconfiguration_simple,simple autoconfiguration>> section for more details.

The CAM is complaining about locked channels
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 * Some viaccess CAMs can have a lock for "mature" channels. To desactivate this lock go on the CAM menu using "gnutv -cammenu" for example (from linuxtv dvb-apps).

You have to set the maturity rating to maximum and unlock Maturity rating in Bolts submenu.

VLC doesn't select the good program even with PAT rewriting
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You also have to rewrite the SDT PID using the `rewrite_sdt` option


[[problems_hp]]
HELP ! my multicast traffic is flooded (I have a HP procurve switch)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The best explanation is found in the HP multicast routing guide.

On switches that do not support Data-Driven IGMP, unregistered multicast
groups are flooded to the VLAN rather than pruned. In this scenario, Fast-Leave IGMP can actually increase the problem of multicast flooding by removing the IGMP group filter before the Querier has recognized the IGMP leave. The Querier will continue to transmit the multicast group during this short time, and because the group is no longer registered the switch will then flood the multicast group to all ports.

On ProCurve switches that do support Data-Driven IGMP (“Smart” IGMP),
when unregistered multicasts are received the switch automatically filters (drops) them. Thus, the sooner the IGMP Leave is processed, the sooner this multicast traffic stops flowing.

Switches without problems (supporting data driven igmp): 

 * Switch 6400cl
 * Switch 6200yl
 * Switch 5400zl
 * Switch 5300xl
 * Switch 4200vl
 * Switch 3500yl
 * Switch 3400cl
 * Switch 2900
 * Switch 2800
 * Switch 2500


Switches WITH problems (NOT supporting data driven igmp): 

 * Switch 2600
 * Switch 2600-PWR
 * Switch 4100gl
 * Switch 6108

So if you have one of the above switches this is "normal". The workaround is to make MuMuDVB join the multicast group. For this put `multicast_auto_join=1` in your configuration file.

MuMuDVB is eating a lot of CPU with sasc-ng !
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you use sasc-ng + dvbloopback, MuMuDVB will eat more CPU than needed.

A part of this CPU time is used to descramble the channels, another part is due to the way dvbloopback is implemented and the way MuMuDVB ask the card.

To reduce the cpu usage, see <<reduce_cpu,reduce MuMuDVB CPU usage>> section. In the case of using MuMuDVB with sasc-ng this improvement can be quite large.


The reception is working but all the channels are down
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the signal is good but MuMuDVB tells you that all the channels are down and you are sure about your pids it can be due to your CAM module if you have one. Try after unplugging your CAM module.

I want to stream from several cards
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The solution is simple: just launch a MuMuDVB process for each card.

I want to stream the whole transponder on one "channel"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

MuMuDVB can stream all the data received by the card to one "channel" (multicast or unicast). In order to do this you have to use the put the PID 8192 in the channel PID list.

I have several network interfaces and I want to choose on which interface the multicast traffic will go
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In order to specify the interface, you can specify a route for the multicast traffic like : 

---------------------------------------------------
route add -net 224.0.0.0 netmask 240.0.0.0 dev eth2
---------------------------------------------------

What does the MuMuDVB error code means ?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Here's a short description of the error codes

------------------------------
    ERROR_ARGS=1,
    ERROR_CONF_FILE,
    ERROR_CONF,
    ERROR_TOO_CHANNELS,
    ERROR_CREATE_FILE,
    ERROR_DEL_FILE,
    ERROR_TUNE,
    ERROR_NO_DIFF,
    ERROR_MEMORY,
    ERROR_NETWORK,
    ERROR_CAM,
    ERROR_GENERIC,
    ERROR_NO_CAM_INIT,
------------------------------



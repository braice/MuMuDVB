MuMuDVB - README for the configuration file
===========================================
Brice Dubost <mumudvb@braice.net>
Version 1.6.1

General behavior
----------------

MuMuDVB needs a configuration file in order to run properly.

The order of the parameters is most of the times not relevant.

You can put comments everywhere in the configuration file: just start the line with `#`. In line comments are not allowed i.e. `port=1234 #The multicast port` is *not* a valid line.

All parameters are in the form: `name=value`

.Example
--------------------------
#The tuning frequency
freq=11987
--------------------------

The configuration file contains two parts

Common part
~~~~~~~~~~~

This is the first part of the configuration file, it contains the parameters needed for tuning the DVB card and the global parameters.

See the <<tuning,tuning>> section for the list of parameters used to tune the card. and <<other_global,other global parameters>> section for parameters like autoconfiguration, cam support etc ...

Channels part
~~~~~~~~~~~~~

If you are not using full autoconfiguration you need to set the list of the channels you want to stream.
Each channel start with an `ip=` or `channel_next` line.


.Example (unicast only)
---------------------------
channel_next
name=Barcelona TV
unicast_port=8090
pids=272
---------------------------

.Example
---------------------------
ip=239.100.0.0
port=1234
name=Barcelona TV
pids=272 256 257 258
---------------------------


See <<channel_parameters,channel parameters>> section for a list of detailled parameters.

Example config files
--------------------

You can find documented examples in the directory `doc/configuration_examples`

[[tuning]]
Parameters concerning the tuning of the card
--------------------------------------------

[NOTE]
For terrestrial and cable, you can use w_scan to see the channels you can receive see <<w_scan, w_scan section>>.
Otherwise you can have a look at the initial tuning files given with linuxtv's dvb-apps.
For european satellite users, you can have a look at http://www.kingofsat.net[King Of Sat]

Parameters concerning all modes (terrestrial, satellite, cable, ATSC)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the following list, only the parameter `freq` is mandatory

[width="80%",cols="2,7,2,3",options="header"]
|==================================================================================================================
|Parameter name |Description | Default value | Comments
|freq | transponder's frequency in MHz  | | Mandatory
|modulation | The kind of modulation used (can be : QPSK QAM16 QAM32 QAM64 QAM128 QAM256 QAMAUTO VSB8 VSB16 8PSK 16APSK 32APSK DQPSK)  | ATSC: VSB_8, cable/terrestrial: QAM_AUTO, satellite: QPSK | Optionnal most of the times
|delivery_system | the delivery system used (can be DVBT DVBS DVBS2 DVBC_ANNEX_AC DVBC_ANNEX_B ATSC) | Undefined | Set it if you want to use the new tuning API (DVB API 5/S2API). Mandatory for DVB-S2
|card | The DVB/ATSC card number | 0 | only limited by your OS
|tuning_timeout |tuning timeout in seconds. | 300 | 0 = no timeout
|timeout_no_diff |If no channels are streamed, MuMuDVB will kill himself after this time (specified in seconds) | 600 |  0 = infinite timeout
|==================================================================================================================




Parameters specific to satellite
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[width="80%",cols="2,6,1,3,2",options="header"]
|==================================================================================================================
|Parameter name |Description | Default value | Possible values | Comments
|pol |transponder's polarisation. One char. 'v' (vertical), 'h' (horizontal), 'l' (left circular), 'r' (right circular) | | h, H, v, V, l, L, r or R | Mandatory
|srate  |transponder's symbol rate | | | Mandatory
|lnb_type |The LNB type | universal | universal, standard | Universal : two local oscilators. Standard : one local oscillator.Most of the LNBs are universal.
|lnb_lof_standard |The frequency of the LNB's local oscillator when lnb_type=standard | 10750 |  | In MHz, see below.
|lnb_slof |The switching frequency frequency of the LNB (define the two bands). Valid when lnb_type=universal | 11700 |  | In MHz, see below.
|lnb_lof_low |The frequency of the LNB's local oscillator for the low band. Valid when lnb_type=universal | 9750 |  | In MHz, see below.
|lnb_lof_high |The frequency of the LNB's local oscillator for the high band. Valid when lnb_type=universal | 10600 |  | In MHz, see below.
|sat_number |The satellite number in case you have multiples lnb, no effect if 0 (only 22kHz tone and 13/18V), send a diseqc message if non 0 | 0 | 1 to 4 | If you have equipment which support more, please contact
|lnb_voltage_off |Force the LNB voltage to be 0V (instead of 13V or 18V). This is useful when your LNB have it's own power supply. | 0 | 0 or 1 | 
|coderate  |coderate, also called FEC | auto | none, 1/2, 2/3, 3/4, 4/5, 5/6, 6/7, 7/8, 8/9, auto |
|rolloff  |rolloff important only for DVB-S2 | 35 | 35, 20, 25, auto | The default value should work most of the times
|==================================================================================================================

Local oscillator frequencies : 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
- S-Band 3650 MHz
- C band (Hi) 5950 MHz
- C band (Lo) 5150 MHz
- Ku Band : this is the default band for MuMuDVB, you don't have to set the LO frequency. For information : Hi band : 10600, Low band : 9750, Single : 10750


Parameters specific to terrestrial (DVB-T)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[NOTE]
`auto` usually works fine for all the parameters except `bandwidth`

[width="80%",cols="2,8,1,4",options="header"]
|==================================================================================================================
|Parameter name |Description | Default value | Possible values
|bandwidth |bandwidth | 8MHz | 8MHz, 7MHz, 6MHz, auto 
|trans_mode |transmission mode | auto | 2k, 8k, auto 
|guardinterval |guard interval | auto |  1/32, 1/16, 1/8, 1/4, auto 
|coderate  |coderate, also called FEC | auto | none, 1/2, 2/3, 3/4, 4/5, 5/6, 6/7, 7/8, 8/9, auto 
|==================================================================================================================

Parameters specific to cable (DVB-C)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[width="80%",cols="2,6,1,3,2",options="header"]
|==================================================================================================================
|Parameter name |Description | Default value | Possible values | Comments
|srate  |transponder's symbol rate | | | Mandatory
|qam |quadrature amplitude modulation | auto | qpsk, 16, 32, 64, 128, 256, auto | This option is obsolete, use modulation instead
|coderate  |coderate, also called FEC | auto | none, 1/2, 2/3, 3/4, 4/5, 5/6, 6/7, 7/8, 8/9, auto  |
|==================================================================================================================

[NOTE]
The http://www.rfcafe.com/references/electrical/spectral-inv.htm[spectral inversion] is fixed to OFF, it should work for most of the people, if you need to change this parameter, please contact.


Parameters specific to ATSC (Cable or Terrestrial)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[width="80%",cols="1,3,1,2,2",options="header"]
|==================================================================================================================
|Parameter name |Description | Default value | Possible values | Comments
|atsc_modulation | the modulation for ATSC | vsb8 | vsb8, vsb16, qam256, qam64, qamauto | This option is obsolete, use modulation instead
|==================================================================================================================

[NOTE]
VSB 8 is the default modulation for most of the terrestrial ATSC transmission


[[other_global]]
Other global parameters
-----------------------

Various parameters
~~~~~~~~~~~~~~~~~~
[width="80%",cols="2,8,1,2,3",options="header"]
|==================================================================================================================
|Parameter name |Description | Default value | Possible values | Comments
|dont_send_scrambled | If set to 1 don't send the packets detected as scrambled. this will also remove indirectly the sap announces for the scrambled channels |0 | |
|show_traffic_interval | the interval in second between two displays of the traffic | 10 | >2 | 
|compute_traffic_interval | the interval in second between two computations of the traffic | 10 | >2 | 
|rtp_header | Send the stream with the rtp headers (execpt for HTTP unicast) | 0 | 0 or 1 | 
|dvr_buffer_size | The size of the "DVR buffer" in packets | 1 | >=1 | see README 
|dvr_thread | Are the packets retrieved from the card in a thread | 0 | 0 or 1 | Experimental, see README 
|dvr_thread_buffer_size | The size of the "DVR thread buffer" in packets | 5000 | >=1 | see README 
|rewrite_pat | Do we rewrite the PAT PID | 0 | 0 or 1 | See README 
|rewrite_sdt | Do we rewrite the SDT PID | 0 | 0 or 1 | See README 
|sort_eit | Do we sort the EIT PID | 0 | 0 or 1 | See README 
|==================================================================================================================

Multicast parameters
~~~~~~~~~~~~~~~~~~~~

[width="80%",cols="2,8,1,2,3",options="header"]
|==================================================================================================================
|Parameter name |Description | Default value | Possible values | Comments
|multicast |Do we activate multicast | 1 | 0 or 1 |
|common_port | Default port for the streaming | 1234 | | 
|multicast_ttl |The multicast Time To Live | 2 | |
|multicast_auto_join | Set to 1 if you want MuMuDVB to join automatically the multicast groups | 0 | 0 or 1 | See known problems in the README
|==================================================================================================================

CAM support parameters
~~~~~~~~~~~~~~~~~~~~~~
[width="80%",cols="2,8,1,2,3",options="header"]
|==================================================================================================================
|Parameter name |Description | Default value | Possible values | Comments
|cam_support |Specify if we wants the support for scrambled channels | 0 | 0 or 1 |
|cam_number |the number of the CAM we want to use | 0 | | In case you have multiple CAMs on one DVB card
|cam_reset_interval |The time (in seconds) we wait for the CAM to be initialised before resetting it. | 30 | | If the reset is not successful, MuMuDVB will reset the CAM again after this interval. The maximum number of resets before exiting is 5
|==================================================================================================================

Autoconfiguration parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[width="80%",cols="2,6,1,2,4",options="header"]
|==================================================================================================================
|Parameter name |Description | Default value | Possible values | Comments
|autoconfiguration |autoconfiguration 1, partial: find audio and video PIDs, 2, full: full autoconfiguration | 0 | 0, 1, 2, partial or full | see the README for more details
|autoconf_ip_header |For full autoconfiguration, the first part of the ip for streamed channel | 239.100 | |  see the README for more details
|autoconf_radios |Do we consider radios as valid channels during full autoconfiguration ? | 0 | 0 or 1 | 
|autoconf_scrambled |Do we consider scrambled channels valid channels during full autoconfiguration ? | 0 | 0 or 1 | Automatic when cam_support=1. Sometimes a clear channel can be marked as scrambled. This option allows you to bypass the ckecking.
|autoconf_pid_update |Do we follow the changes in the PIDs when the PMT is updated ? | 1 | 0 or 1 | 
|autoconf_unicast_start_port |The unicast port for the first discovered channel |  |  | See README for more details.
|autoconf_tsid_list | If you don't want to configure all the channels of the transponder in full autoconfiguration mode, specify with this option the list of the transport stream ids of the channels you want to autoconfigure. | empty |  | 
|autoconf_name_template | The template for the channel name, ex `%number-%name` | empty | | See README for more details
|==================================================================================================================

SAP announces parameters
~~~~~~~~~~~~~~~~~~~~~~~~
[width="80%",cols="2,8,1,2,3",options="header"]
|==================================================================================================================
|Parameter name |Description | Default value | Possible values | Comments
|sap | Generation of SAP announces | 0 (1 if full autoconfiguration) | 0 or 1 | 
|sap_organisation |Organisation field sent in the SAP announces | MuMuDVB | | Optionnal
|sap_uri |URI  field sent in the SAP announces |  | | Optionnal
|sap_sending_ip |The SAP sender IP address | 0.0.0.0 | | Optionnal, not autodetected
|sap_interval |Interval in seconds between sap announces | 5 | positive integers | 
|sap_default_group | The default playlist group for sap announces | | string | Optionnal. You can use the keyword %type, see README
|sap_ttl |The TTL for the multicast SAP packets | 255 |  | The RFC 2974 says "SAP announcements ... SHOULD be sent with an IP time-to-live of 255 (the use of TTL scoping for multicast is discouraged [RFC 2365])."
|==================================================================================================================

HTTP unicast parameters
~~~~~~~~~~~~~~~~~~~~~~~
[width="80%",cols="2,8,1,2,3",options="header"]
|==================================================================================================================
|Parameter name |Description | Default value | Possible values | Comments
|unicast |Set this option to one to activate HTTP unicast | 0  |  | see the README for more details
|ip_http |the listening ip for http unicast, if you want to listen to all interfaces put 0.0.0.0 | 0.0.0.0  |  | see the README for more details
|port_http | The listening port for http unicast | 4242 | |  see the README for more details
|unicast_consecutive_errors_timeout | The timeout for disconnecting a client wich is not responding | 5 | | A client will be disconnected if not data have been sucessfully sent during this interval. A value of 0 desactivate the timeout (unadvised).
|unicast_max_clients | The limit on the number of connected clients | 0 | | 0 : no limit.
|unicast_queue_size | The maximum size of the buffering when writting to a client fails | 512kBytes | | in Bytes.
|==================================================================================================================


[[channel_parameters]]
Channel parameters
------------------

Each channel start with an `ip=` or `channel_next` line. The only other mandatory parameter is the `name` of the channel.

Concerning the PIDs see the <<getpids,getting the PIDs>> section


[width="80%",cols="2,8,1,1,3",options="header"]
|==================================================================================================================
|Parameter name |Description | Default value | Possible values | Comments
|ip |multicast (can also be unicast, in raw UDP ) ip where the chanel will be streamed | | | Optionnal if you set multicast=0 (if not used you must use channel_next)
|port | The port | 1234 or common_port | | Ports below 1024 needs root rights.
|unicast_port | The HTTP unicast port for this channel | | | Ports below 1024 needs root rights. You need to activate HTTP unicast with `ip_http`
|sap_group |The playlist group for SAP announces | | string | optionnal
|cam_pmt_pid |Only for scrambled channels. The PMT PID for CAM support | | | 
|ts_id |The transport stream id (program number), olny for autoconfiguration, or rewrite (PAT or SDT) see README for more details | | | 
|name | The name of the channel. Will be used for /var/run/mumudvb/chaines_diffusees%d, logging and SAP announces | | | Mandatory
|pids | The PIDs list, separated by spaces | | | some pids are always sent (PAT CAT EIT SDT TDT NIT), see README for more details
|==================================================================================================================



[[getpids]]
Get the PID numbers
-------------------

The simplest way is to use autoconfiguration and modify the generated configuration file : `/var/run/mumudvb/mumudvb_generated_conf_card%d`

You use autoconfiguration
~~~~~~~~~~~~~~~~~~~~~~~~~

If you use full autoconfiguration, you don't need to specify any channel and don't need any PID, this section does not concern you.

If you use partial autoconfiguration, you'll need only the PMT PID for each channel.

You do not use autoconfiguration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you don't use autoconfiguration (see the README), you have to get the PIDs (Program Identifier) for each channel.

For each channel it is advised to specify at least :
- One video PID (except for radios)
- One audio PID
- The PMT PID
- The PCR PID (if different from video/audio)

If you don't have access to the PIDs via a website like http://www.kingofsat.net[King Of Sat], the easiest way is to use linuxtv's dvb-apps.


You don't know on wich frequency to tune and the channels you can receive. In this case, you can use <<w_scan,w_scan>> or using <<scan_inital_tuning,scan>> from dvb-apps if you have an initial tuning config file.

[[w_scan]]
Using w_scan to get an initial tuning file
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
[NOTE]
w_scan works only for terrestrial or cable.

You can find wscan in the http://edafe.org/vdr/w_scan/[w_scan website].

Once you compiled it (optionnal for x86), you can lauch it using 
---------------------------------------
./w_scan -x >> inital_tuning_file.txt
---------------------------------------

[NOTE]
If you are not scanning DVB-T you will have to set the option
-------------------------------
-f type	frontend type
 What programs do you want to search for?
  a = atsc (vsb/qam)
  c = cable
  t = terrestrian [default]
-------------------------------

Your `initial_tuning_file.txt` file should contain lines like 

--------------------------------------------
T 482000000 8MHz AUTO AUTO AUTO AUTO AUTO AUTO
T 498000000 8MHz AUTO AUTO AUTO AUTO AUTO AUTO
T 514000000 8MHz 2/3 2/3 QAM64 8k 1/4 NONE
--------------------------------------------

If you want to use full autoconfiguration, this file contains all the parameters you need. For example the second row is the frequency.

The format is : 
.Terrestrial
----------------------------------------------------------------------------------------
T freq bandwidth fec_hi fec_lo modulation transmission-mode guard-interval hierarchy
----------------------------------------------------------------------------------------

.Cable
--------------------------------------------
C freq symbol_rate fec modulation
--------------------------------------------

.ATSC
--------------------------------------------
A freq modulation
--------------------------------------------

If you don't plan to use full autoconfiguration you can use `scan` with this file, see <<scan_inital_tuning, using scan with an inital tuning file>>

[[scan_inital_tuning]]
Using scan with an initial tuning file
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

[NOTE]
With satellite this allow you to find all the frequencies (if the broadcaster follow the norm). Because, every transponder announces the others.

You need `scan` from linuxtv's dvb-apps

If you have an initial tuning file from `w_scan`, the scan documentation or other type : 

--------------------------------------------------------
scan -o pids pathtoyourinitialtuningfile
--------------------------------------------------------

You'll first get blocks like 

----------------------------------------------------------------------------------------------------------------
>>> tune to: 514000000:INVERSION_AUTO:BANDWIDTH_8_MHZ:FEC_2_3:FEC_2_3:QAM_64:TRANSMISSION_MODE_8K:GUARD_INTERVAL_1_4:HIERARCHY_NONE
0x0000 0x7850: pmt_pid 0x0110 Barcelona TV -- Barcelona TV (running)
0x0000 0x7851: pmt_pid 0x0710 COM Radio -- COM Radio (running)
0x0000 0x7855: pmt_pid 0x0210 TV L'Hospitalet -- TV L'Hospitalet (running)
0x0000 0x7856: pmt_pid 0x0510 Radio Hospitalet -- Radio Hospitalet (running)
0x0000 0x785a: pmt_pid 0x0310 Televisio Badalona -- Televisio Badalona (running)
0x0000 0x785b: pmt_pid 0x0610 Radio Ciutat Badalona -- Radio Ciutat Badal
----------------------------------------------------------------------------------------------------------------

You have now acces to the PMT PID (in hexadecimal), you can convert it to decimal and use partial autoconfiguration.

After this blocks, you'll get lines like

----------------------------------------------------------------------------------------------------------------
Sensacio FM              (0x273f) 02: PCR == A            A 0x0701      
urBe TV                  (0x7864) 01: PCR == V   V 0x0300 A 0x0301 (cat)
Canal Catala Barcelona   (0x7869) 01: PCR == V   V 0x0200 A 0x0201 (cat)
25 TV                    (0x786e) 01: PCR == V   V 0x0400 A 0x0401 (spa) TT 0x0402
ONDA RAMBLA PUNTO RADIO  (0x786f) 02: PCR == A            A 0x0601 (cat)
Localia                  (0x7873) 01: PCR == V   V 0x0100 A 0x0101      
ONA FM                   (0x7874) 02: PCR == A            A 0x0501      
TV3                      (0x0321) 01: PCR == V   V 0x006f A 0x0070 (cat) 0x0072 (vo) 0x0074 (ad) TT 0x0071 AC3 0x0073 SUB 0x032b
----------------------------------------------------------------------------------------------------------------

You have now acces to the other PIDs

MuMuDVB wants the PIDs in decimal, so you have to convert the pids from hexadecimal to decimal.


Scan only one transponder
^^^^^^^^^^^^^^^^^^^^^^^^^

You first have to tune the card on the wanted frequency (with `tune`, `szap` or `tzap` for example).

After you use the scan utility:

----------------------
scan -o pids -c -a 0
----------------------

Where 0 is the card number

And you'll get results like in the section <<scan_initial_tuning,scan with an initial tuning file>>



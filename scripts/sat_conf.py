#! /usr/bin/env python
# -*- coding: iso8859-15 -*-

from sat_base import *
from socket import gethostname

host = gethostname()

conf = []

if host == 'canard' : 
    conf = [ Hotbird_12476(0) ,
             Hotbird_10911(1) ,
             Hotbird_12597(2) ,
	     Hotbird_12577(3) ]
if host == 'lapin' :
    conf = [ Hotbird_10796(0),
             Hotbird_11604(1),
             TNT_R1_586000(2)]
if host == 'oie' : 
    conf = [ Hotbird_12245(0) ,
             Hotbird_11137(1) ,
             Hotbird_10873(2) ,
             Hotbird_11304(3) ,
             Hotbird_11623(4) ]


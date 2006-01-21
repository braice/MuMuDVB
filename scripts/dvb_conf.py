#! /usr/bin/env python
# -*- coding: iso-8859-15 -*-

""" Défini les transpondeurs à associer à chacune des cartes """

from dvb_base import *
from socket import gethostname

host = gethostname()

transpondeurs = { 'canard' : [ TNT_R4_498000(0) ,
                               Hotbird_12476(1) ,
                               Hotbird_11242(2) ,
                               Hotbird_12597(3) ,
                               Hotbird_12577(4) ],
                  'lapin' :  [ Hotbird_10911(0) ,
                               Hotbird_11604(1) ,
                               TNT_R1_586000(2) ,
                               TNT_R6_562000(3) ,
                               TNT_R2_474000(4) ,
                               TNT_R3_522000(5) ],
                  'oie' : [    Hotbird_12245(0) ,
                               Hotbird_11137(1) ,
                               Hotbird_10873(2) ,
                               Hotbird_11304(3) ,
                               Hotbird_11623(4) ]
                               }

conf = transpondeurs.get(host,[])
                               
if __name__ == '__main__' :
    import sys
    if len(sys.argv) == 2 :
        conf = transpondeurs.get(sys.argv[1],[])
    for t in conf :
        print t
        for chaine in t.chaines.values() :
            print '\t%s' % chaine

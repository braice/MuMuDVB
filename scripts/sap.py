#! /usr/bin/env python
# -*- coding: iso-8859-15 -*-

""" Génération de la configuration sap à partir des données de mumudvb
    Lancement du serveur sap.
    
    Option -d pour que le programme ne se lance pas en daemon. """

import os, sys, signal
from socket import getfqdn
from time import sleep
from commands import getoutput

sys.path.append('/usr/scripts/python-lib')
import lock
from daemon import daemonize

################CONFIG

SAP_CONF = '/etc/sap.cfg'
LOCK='/var/run/mumudvb/sap'
CHAINES_DIFFUSES = '/var/run/mumudvb/chaines_diffusees_carte%d'
base_conf = """##################################
# Fichier généré automatiquement #
# NE PAS EDITER                  #
##################################
[global]
sap_ttl=4
sap_ipversion=4
sap_delay=10
ipv6_scope=8
"""

chaine_template="""
[program]
name=%s
user=crans
machine=%s
site=http://www.crans.org
address=%s
port=%s
program_ttl=4
program_ipversion=4
playlist_group=%s
""" % ( '%(nom)s', getfqdn(), '%(ip)s', '%(port)s', '%(langue)s' )

################FIN CONFIG

# Variables globales, ne pas toucher
sap_pid = 0
sum = 0
class no_data(Exception):
    pass

def gen_sap() :
    """ Génération du ficher de conf du sap """
    data = []
    for i in range(0,6) :
        try :
            data.append(open(CHAINES_DIFFUSES % i).readlines())
        except IOError :
            continue

    file = open(SAP_CONF,'w')
    file.write(base_conf)
    ok=0
    for fichier in data :
        for line in fichier :
            ip, port, nom_chaine = line.strip().split(':')
            file.write(chaine_template % { 
                'nom'  : nom_chaine,
                'langue': nom_chaine.split()[0],
                'ip'   : ip,
                'port' : port} )
            ok=1
    file.close()
    if not ok : raise no_data

def is_alive(child_pid) :
    """ Vérifie si le processus fils tourne """
    try :
        if os.waitpid(child_pid,1) != (0,0) :
            raise OSError
        return True
    except OSError :
        return False

def term(a=None,b=None) :
    """ Tue le serveur sap puis quitte """
    kill()
    lock.remove_lock(LOCK)
    sys.exit(0)
    
def kill() :
    """ Tue le serveur sap """
    if not is_alive(sap_pid) :
        return
    print "Kill sap"
    os.kill(sap_pid,15)
    sleep(1)
    if is_alive(sap_pid) :
        # Salloperie
        print "WARNING : sap résitant"
        os.kill(sap_pid,9)
        sleep(1)
        
if __name__ == '__main__' :
    # Arguments
    if '-d' not in sys.argv :
        daemonize()

    # Lock
    lock.make_lock(LOCK,"Serveur SAP")
    
    # Signal handler
    signal.signal(signal.SIGTERM,term)
    signal.signal(signal.SIGINT,term)
    
    while 1 :
        try :
            gen_sap()
        except no_data :
            # Rien n'est diffusé
            kill()
        else :
            # Changent de config ?
            new_sum = getoutput('md5sum %s' % SAP_CONF).split()[0]
            if new_sum != sum :
                print "Reconfiguration"
                sum = new_sum
                if sap_pid>1 and is_alive(sap_pid) :
                    kill()
            if not is_alive(sap_pid) :
                # Ne tourne pas, on relance
                sap_pid = os.spawnl(os.P_NOWAIT,'/usr/local/bin/sapserver','sap','-f',SAP_CONF)
                print "Lancement serveur, pid=%s" % sap_pid

        sleep(60)

MuMuDVB - README concernant le fichier de configuration
=======================================================
Brice Dubost <mumudvb@braice.net>
Version 1.6.1

Comportement général
---------------------

Pour fonctionner correctement, MuMuDVB a besoin d'un fichier de configuration.

L'ordre des paramètres n'a, la plupart du temps, pas d'importance.

Vous pouvez mettre des commentaires n'importe où dans le fichier de configuration, il suffit de commencer la ligne par un `#`. Les commentaires dans une ligne ne sont pas autorisés, exemple `port=1234 #Le port multicast` n'est pas une ligne valide.

Tous les paramètres s'écrivent sous la forme `nom=valeur`

.Example
--------------------------
#La fréquence d'accord
freq=11987
--------------------------

Le fichier de configurations est constitué de deux parts : une partie générale, et une partie définissant les chaînes


Partie générale
~~~~~~~~~~~~~~~

Cette partie, la première du fichier de configuration, contient les paramètres nécessaires pour accorder la carte DVB et les autres paramètres globaux.

Pour les paramètres concernant l'accord de la carte, référez vous à la section <<tuning,paramètres d'accord>>. Pour les autres paramètres globaux, référez vous à la section <<other_global,paramètres globaux>>

Partie concernant les chaînes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Si vous n'utilisez pas l'autoconfiguration complète, vous devez définir les chaînes que vous voulez diffuser.

Chaque définition de chaîne commence par une ligne `ip=` ou `channel_next`.



.Example
------------------------------
channel_next
name=Barcelona TV
unicast_port=8090
pids=272
------------------------------


.Example
------------------------------
ip=239.100.0.0
port=1234
name=Barcelona TV
pids=272 256 257 258
------------------------------

Référez vous a la section <<channel_parameters,configuration des chaînes>> pour une liste détaillée des différents paramètres.


Fichiers d'exemple
------------------

Vous pouvez trouver des fichiers de configuration documentés dans le répertoire  `doc/configuration_examples`

[[tuning]]
Paramètres concernant l'accord de la carte
------------------------------------------

[NOTE]
Pour la réception terrestre et câblée, vous pouvez utiliser w_scan pour savoir quelles chaînes vous pouvez recevoir. Pour plus de détails référez vous a la section <<w_scan, w_scan>>. Sinon vous pouvez regarder le contenu des "initial tuning files" fourni avec dvb-apps de linuxtv.
Le site  http://www.kingofsat.net[King Of Sat] référence les chaînes satellite pouvant être reçues en Europe


Paramètres communs à tous les modes de réception (terrestre, satellite, câble et ATSC)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Dans la liste suivante, seul le paramètre `freq` est obligatoire

[width="80%",cols="2,7,2,3",options="header"]
|==================================================================================================================
|Nom |Description | Valeur par défaut | Commentaires
|freq | Fréquence du transpondeur en MHz  | | Obligatoire
|modulation | Le type de modulation utilisé (valeurs possibles : QPSK QAM16 QAM32 QAM64 QAM128 QAM256 QAMAUTO VSB8 VSB16 8PSK 16APSK 32APSK DQPSK)  | ATSC: VSB_8, cable/terrestrial: QAM_AUTO, satellite: QPSK | Optionnel la plupart du temps
|delivery_system | Le type de système utilisé (valeurs possibles : DVBT DVBS DVBS2 DVBC_ANNEX_AC DVBC_ANNEX_B ATSC) | Non défini | Spécifiez le si vous voulez utiliser la nouvelle API pour l'accord des cartes (DVB API 5/S2API). Obligatoire pour le DVB-S2
|card | Le numéro de carte DVB/ATSC| 0 | Limité seulement par l'OS
|card_dev_path | Le chemin vers le répertoire contenant les "devices" DVB. Utilisez cette option si vous utilisez des chemins personalisés comme /dev/dvb/card_astra | /dev/dvb/adapter%d | 
|tuning_timeout | Temps d'attente pour l'accord de la carte | 300 | 0 = attente infinie
|timeout_no_diff | Si aucune chaîne n'est diffusée, MuMuDVB se "suicidera" au bout de ce délai ( en secondes ) | 600 |  0 = attente infinie
|==================================================================================================================


Paramètres spécifiques à la réception satellite
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[width="80%",cols="2,6,1,3,2",options="header"]
|==================================================================================================================
|Nom |Description | Valeur par défaut | Valeurs possibles | Commentaires
|pol |Polarisation. Un caractère. 'v' (verticale), 'h' (horizontale), 'l' ( circulaire gauche ), 'r' ( circulaire droite ) | | h, H, v, V, l, L, r ou R | Obligatoire
|srate  |Le taux de symboles ( symbol rate ) | | | Obligatoire
|lnb_type |Le type de LNB | universal | universal, standard | Universel : deux oscillateurs locaux. Standard : un oscillateur local. La plupart des LNBs sont universels.
|lnb_lof_standard |La fréquence de l'oscillateur local du LNB ( lorsque le type de LNB est standard ) | 10750 |  | En MHz, voir plus bas
|lnb_slof |La fréquence de commutation pour le LNB, définit les bandes haute et basse ( lorsque le type de LNB est universel ) | 11700 |  |En MHz, voir plus bas
|lnb_lof_low |La fréquence de l'oscillateur local du LNB pour la bande basse ( lorsque le type de LNB est universel )  | 9750 |  |En MHz, voir plus bas
|lnb_lof_high |La fréquence de l'oscillateur local du LNB pour la bande haute ( lorsque le type de LNB est universel ) | 10600 |  | En MHz, voir plus bas
|sat_number | Le numéro de satellite si vous avez plusieurs LNB. Aucun effet si égal à 0 (seulement ton 22kHz et 13/18V), envoie un message diseqc si différent de 0 | 0 | 0, 1 à 4 | 
|lnb_voltage_off |Force la tension pour le LNB à 0V (au lieu de 13V ou 18V). Ceci est utile si votre LNB possède sa propre alimentation. | 0 | 0 ou 1 | 
|coderate  |coderate, appelé aussi FEC | auto | none, 1/2, 2/3, 3/4, 4/5, 5/6, 6/7, 7/8, 8/9, auto | 
|rolloff  |rolloff important seulement pour le DVB-S2 | 35 | 35, 20, 25, auto | La valeur par défaut devrait marcher la plupart du temps
|==================================================================================================================

Fréquences pour l'oscillateur local : 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
- Bande S 3650 MHz
- Bande C (Hi) 5950 MHz
- Bande C (Lo) 5150 MHz
- Bande Ku : C'est la bande par défaut, vous n'avez pas a spécifier la fréquence de l'oscillateur local. Pour information Hi band : 10600, Low band : 9750, Single : 10750


Paramètres spécifiques à la réception terrestre (DVB-T)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[NOTE]
`auto` marche habituellement bien excepté pour `bandwidth`

[width="80%",cols="2,8,1,4",options="header"]
|==================================================================================================================
|Nom |Description | Valeur par défaut | Valeurs possibles
|bandwidth | Largeur de bande | 8MHz | 8MHz, 7MHz, 6MHz, auto 
|trans_mode |Mode de transmission | auto | 2k, 8k, auto 
|guardinterval |Intervalle de garde | auto |  1/32, 1/16, 1/8, 1/4, auto 
|coderate  |coderate, aussi appelé FEC | auto | none, 1/2, 2/3, 3/4, 4/5, 5/6, 6/7, 7/8, 8/9, auto 
|==================================================================================================================

Paramètres spécifiques a la réception par câble (DVB-C)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[width="80%",cols="2,6,1,3,2",options="header"]
|==================================================================================================================
|Nom |Description | Valeur par défaut | Valeurs possibles | Commentaires
|srate  |Le taux de symboles (Symbol rate)| | | obligatoire
|qam | Modulation : quadrature amplitude modulation | auto | qpsk, 16, 32, 64, 128, 256, auto | Cette option est obsolète, utilisez le paramètre modulation
|coderate  |coderate aussi appelé FEC | auto | none, 1/2, 2/3, 3/4, 4/5, 5/6, 6/7, 7/8, 8/9, auto  |
|==================================================================================================================

[NOTE]
http://www.rfcafe.com/references/electrical/spectral-inv.htm[L'inversion spectrale] est fixée à OFF, ceci doit fonctionner pour la plupart des utilisateurs. Merci de contacter si vous avez besoin de changer ce paramètre.

Paramètres pour la réception ATSC (Câble ou Terrestre)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[width="80%",cols="1,3,1,2,2",options="header"]
|==================================================================================================================
|Nom |Description | Valeur par défaut | Valeurs possibles  | Commentaires
|atsc_modulation | La modulation | vsb8 | vsb8, vsb16, qam256, qam64, qamauto | Cette option est obsolète, utilisez le paramètre modulation
|==================================================================================================================

[NOTE]
VSB 8 est la modulation par défaut pour la plupart des diffuseurs ATSC terrestre.


[[other_global]]
Autres paramètres globaux
-------------------------

Paramètres divers
~~~~~~~~~~~~~~~~~

[width="80%",cols="2,8,1,2,3",options="header"]
|==================================================================================================================
|Nom |Description | Valeur par défaut | Valeurs possibles | Commentaires
|dont_send_scrambled | Si positionné à 1, MuMuDVB n'enverra pas les paquets brouillés. Cela retirera (indirectement) les annonces SAP pour les chaînes brouillées |0 | |
|show_traffic_interval | Le temps en secondes entre deux affichages du trafic | 10 | >2 | 
|compute_traffic_interval | Le temps en secondes entre deux calculs du trafic | 10 | >2 | 
|rtp_header | Envoie les en-têtes RTP avec le flux (excepté pour l'unicast HTTP) | 0 | 0 ou 1 | 
|dvr_buffer_size | La taille du  "tampon DVR" en paquets de 188 octets | 1 | >=1 | Se référer au README 
|dvr_thread | Est ce que les packets sont reçus par un thread ? | 0 | 0 ou 1 | Fonctionnalité "expérimentale", se référer au README 
|dvr_thread_buffer_size | La taille du tampon pour le thread en packets de 188 octets | 5000 | >=1 | se référer au README 
|rewrite_pat | Est ce que MuMuDVB doit réécrire le PID PAT | 0 | 0 ou 1 | cf README 
|rewrite_sdt | Est ce que MuMuDVB doit réécrire le PID SDT | 0 | 0 ou 1 | cf README 
|sort_eit | Est ce que MuMuDVB doit trier les PID EIT | 0 | 0 ou 1 | cf README 
|==================================================================================================================

Paramètres concernant le multicast
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
[width="80%",cols="2,8,1,2,3",options="header"]
|==================================================================================================================
|Nom |Description | Valeur par défaut | Valeurs possibles | Commentaires
|multicast | Est ce que le multicast est activé ? | 1 | 0 ou 1 | 
|common_port | Le port par défaut pour la diffusion multicast | 1234 | | 
|multicast_ttl | Le TTL multicast | 2 | |
|multicast_auto_join | Si positionné à 1 MuMuDVB joindra automatiquement tous les groupes multicast créés | 0 | 0 or 1 | cf problèmes connus dans le README
|==================================================================================================================



Paramètres concernant le support des cartes CAM
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[width="80%",cols="2,8,1,2,3",options="header"]
|==================================================================================================================
|Nom |Description | Valeur par défaut | Valeurs possibles | Commentaires
|cam_support |Indique si l'on veux le support pour les chaînes brouillées | 0 | 0 ou 1 |
|cam_number |Le numéro du module CAM que l'on veux utiliser| 0 | | Dans le cas ou vouz avez plusieurs modules CAM sur une carte DVB
|cam_reset_interval |Le temps (en secondes) que MuMuDVB attends pour que la CAM soit initialisée. Après ce délai, MuMuDVB tentera de réinitialiser le module CAM. | 30 | | Si la réinitialisation échoue, MuMuDVB retentera de réinitialiser le module après cet intervelle de temps. Le nombre maximum de tentatives de réinitialisations avant de quitter est 5.
|==================================================================================================================

Paramètres pour l'autoconfiguration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[width="80%",cols="3,5,1,2,5",options="header"]
|==================================================================================================================
|Nom |Description | Valeur par défaut | Valeurs possibles | Commentaires
|autoconfiguration |autoconfiguration 1, partial: Trouve les PIDs audio et video, 2, full: autoconfiguration complète | 0 | 0, 1, 2, partial ou full | Se référer au README pour plus de détails
|autoconf_ip_header |Pour l'autoconfiguration complète, la première partie de l'ip des chaînes diffusées | 239.100 | | Se référer au README pour plus de détails
|autoconf_radios | Lors de l'autoconfiguration complète, est ce que les radios seront diffusées ?| 0 | 0 ou 1 | 
|autoconf_scrambled |Lors de l'autoconfiguration complète, est ce que les chaînes brouillées seront diffusées ? | 0 | 0 or 1 | Automatique lorsque cam_support=1. Parfois, une chaîne en clair peut être marquée comme étant cryptée. Cette option est aussi nécessaire lorsqu'une softcam est utilisée.
|autoconf_pid_update |Est ce que MuMuDVB se reconfigure lorsque le PMT est mis à jour ? | 1 | 0 or 1 | 
|autoconf_unicast_start_port |Le port unicast pour la première chaine découverte |  |  | Voir README-fr pour plus de détails.
|autoconf_unicast_port |Le port unicast pour chaque chaine. Ex "2000+%number" (autoconfiguration complète) |  |  | Vous pouvez utiliser des expressions contenant `+` `*` `%card` et `%number`. Ex : `autoconf_unicast_port=2000+100*%card+%number`
|autoconf_sid_list |Pour ne pas autoconfigurer toutes les chaînes du transpondeur en autoconfiguration complète, spécifiez avec cette option la liste des service id (numeros de programme) des chaînes que vous voulez configurer | vide |  | 
|autoconf_name_template |Le modèle pour le nom des chaînes en autoconfiguration complète, ex `%number-%name` | vide |  | Voir README-fr pour plus de détails.
|==================================================================================================================

Paramètres concernant les annonces SAP
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[width="80%",cols="2,6,1,2,5",options="header"]
|==================================================================================================================
|Nom |Description | Valeur par défaut | Valeurs possibles | Commentaires
|sap | Génération des annonces SAP | 0 (1 si autoconfiguration complète) | 0 or 1 | 
|sap_organisation |Champ "organisation" envoyé avec les annonces SAP | MuMuDVB | | Optionnel
|sap_uri |Champ "URI" envoyé avec les annonces SAP |  | | Optionnel
|sap_sending_ip |L'IP d'envoi des annonces SAP | 0.0.0.0 | | Optionnel, non détecté automatiquement
|sap_interval |Intervalle en secondes entre les annonces SAP | 5 | entiers positifs | 
|sap_default_group | Le groupe de liste de lecture par défaut pour les annonces SAP | | | Optionnel. Vous pouvez utiliser le mot clef %type, voir le README-fr pour plus de détails
|sap_ttl |Le TTL pour les paquets SAP multicast | 255 |  | RFC 2974 : "SAP announcements ... SHOULD be sent with an IP time-to-live of 255 (the use of TTL scoping for multicast is discouraged [RFC 2365])."
|==================================================================================================================

Paramètres concernant l'unicast HTTP
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[width="80%",cols="2,8,1,5",options="header"]
|==================================================================================================================
|Nom |Description | Valeur par défaut | Commentaires
|unicast | Utilisez cette option pour activer l'unicast HTTP | 0  |   se référer au README pour plus de détails
|ip_http | L'ip d'écoute du serveur unicast. Si vous voulez écouter sur toutes les interfaces mettez 0.0.0.0 | 0.0.0.0  |    se référer au README pour plus de détails.
|port_http | Le port d'écoute pour l'unicast HTTP | 4242 |  Vous pouvez utiliser des expressions mathématiques contenant des entiers, * et +. Vous pouvex utiliser le mot clef %card. Ex `port_http=2000+%card*100`
|unicast_consecutive_errors_timeout | Le délai pour déconnecter un client qui ne réponds pas | 5 |  Un client sera déconnecté si aucune donnée n'a été envoyée avec succès durant cet intervalle. Une valeur 0 désactive cette fonctionnalité (déconseillé).
|unicast_max_clients | Limite sur le nombre de clients simultanés | 0 |  0 : pas de limite.
|unicast_queue_size | La taille maximum du tampon utilisé lorsque l'écriture pour un client échoue. | 512k octets|  en octets.
|==================================================================================================================


[[channel_parameters]]
Paramètres concernant les chaînes
---------------------------------

Chaque définition de chaîne commence par une ligne `ip=` ou `channel_next`. Le seul autre paramètre obligatoire est le paramètre `name`.

Concernant les PIDs, référez vous à la section <<getpids,obtenir les PIDs>>.


[width="80%",cols="2,8,1,4",options="header"]
|==================================================================================================================
|Nom |Description | Valeur par défaut |  Commentaires
|ip |Adresse ip multicast sur laquelle la chaîne sera diffusée | |  Optionnel si multicast=0 (si non utilisé, channel_next doit être utilisé à la place)
|port | Le port | 1234 ou common_port |  Les ports inférieurs à 1024 nécessitent les droits root.
|unicast_port | Le port pour l'unicast HTTP ( associé à cette chaîne ) | | Les ports inférieurs à 1024 nécessitent les droits root. Vous devez activer l'unicast HTTP avec l'option `ip_http`
|sap_group | Le groupe de liste de lecture pour les annonces SAP | |  optionnel
|cam_pmt_pid |Uniquement pour les chaînes brouillées. Le PID PMT pour le module CAM | |  
|service_id |Le  "service id" (appelé aussi "program number"), uniquement pour l'autoconfiguration ou la réécriture du PID PAT ou SDT, se référer au README pour plus de détails | | 
|name | Le nom de la chaîne. Sera utilisé pour /var/run/mumudvb/chaines_diffusees%d, les journaux et les annonces SAP | | Obligatoire
|pids | La liste des PIDs, séparés par des espaces. | |  Certains PIDs sont systématiquement envoyés (PAT CAT EIT SDT TDT NIT).
|==================================================================================================================



[[getpids]]
Obtenir les PIDs
----------------

La manière la plus simple est d'utiliser l'autoconfiguration et de modifier le fichier de configuration généré : `/var/run/mumudvb/mumudvb_generated_conf_card%d`

Vous utilisez l'autoconfiguration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Si vous utilisez l'autoconfiguration complète, vous n'avez à spécifier aucune chaîne et vous n'avez besoin de spécifier aucun PID, cette section ne vous concerne donc pas.

Si vous utilisez l'autoconfiguration partielle, vous aurez besoin seulement du PID PMT pour chaque chaîne, lisez la suite pour savoir comment l'obtenir.

Vous n'utilisez pas l'autoconfiguration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Si vous n'utilisez pas l'autoconfiguration, vous devez obtenir les PIDs (Program Identifier) pour chaque chaîne.

Pour chaque chaîne, il est conseillé de spécifier au minimum :

- Un PID vidéo (sauf pour les radios)
- Un PID audio
- Le PID PMT
- Le PID PCR (si différent du PID audio/video)

Si vous n'avez pas accès aux PIDs via un site web comme http://www.kingofsat.net[King Of Sat], la manière la plus facile pour les obtenir est d'utiliser les dvb-apps de linuxtv.

Si vous ne savez pas sur quelle fréquence accorder votre carte ou les chaînes que vous pouvez recevoir, vous pouvez utiliser <<w_scan,w_scan>> ou <<scan_inital_tuning,scan>> (des dvb-apps) si vous avez un fichier d'accord initial (généralement fourni avec la documentation de scan).

[[w_scan]]
Utiliser w_scan pour obtenir un fichier d'accord initial
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
[NOTE]
w_scan fonctionne uniquement pour la réception terrestre ou câblée.

Vous pouvez obtenir w_scan à partir du http://edafe.org/vdr/w_scan/[site web de w_scan].

Une fois que vous l'avez compilé (facultatif pour les x86), vous pouvez le lancer en utilisant  
---------------------------------------
./w_scan -x >> inital_tuning_file.txt
---------------------------------------

[NOTE]
Si vous n'utilisez pas une carte DVB-T vous aurez a définir l'option 
-------------------------------
-f type	frontend type
 What programs do you want to search for?
  a = atsc (vsb/qam)
  c = cable
  t = terrestrian [default]
-------------------------------

Votre fichier `initial_tuning_file.txt` devrai contenir des lignes similaires à 

--------------------------------------------
T 482000000 8MHz AUTO AUTO AUTO AUTO AUTO AUTO
T 498000000 8MHz AUTO AUTO AUTO AUTO AUTO AUTO
T 514000000 8MHz 2/3 2/3 QAM64 8k 1/4 NONE
--------------------------------------------

Si vous voulez utiliser l'autoconfiguration complète, ce fichier contient toutes les informations nécessaires. Par exemple, la seconde colonne est la fréquence ( en Hz ).

Le format est le suivant : 

.Terrestre
----------------------------------------------------------------------------------------
T freq bandwidth fec_hi fec_lo modulation transmission-mode guard-interval hierarchy
----------------------------------------------------------------------------------------

.Câble
--------------------------------------------
C freq symbol_rate fec modulation
--------------------------------------------

.ATSC
--------------------------------------------
A freq modulation
--------------------------------------------

Si vous ne voulez pas utiliser l'autoconfiguration complète, vous pouvez utiliser `scan` avec ce fichier, voir la section  <<scan_inital_tuning, utiliser scan avec un fichier d'accord initial>>.

[[scan_inital_tuning]]
Utiliser scan avec un fichier d'accord initial
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

[NOTE]
En réception satellite, ceci vous permet de trouver toutes les fréquences (si le diffuseur suit la norme). En effet chaque transpondeur annonce les autres.

Dans la suite vous aurez besoin de l'utilitaire `scan` des dvb-apps.

Si vous avez un fichier d'accord initial de `w_scan`, de la documentation de scan ou tout autre source, tapez : 

--------------------------------------------------------
scan -o pids cheminversvotrefichierdaccordinitial
--------------------------------------------------------

Vous allez d'abord obtenir des blocs comme :  

----------------------------------------------------------------------------------------------------------------
>>> tune to: 514000000:INVERSION_AUTO:BANDWIDTH_8_MHZ:FEC_2_3:FEC_2_3:QAM_64:TRANSMISSION_MODE_8K:GUARD_INTERVAL_1_4:HIERARCHY_NONE
0x0000 0x7850: pmt_pid 0x0110 Barcelona TV -- Barcelona TV (running)
0x0000 0x7851: pmt_pid 0x0710 COM Radio -- COM Radio (running)
0x0000 0x7855: pmt_pid 0x0210 TV L'Hospitalet -- TV L'Hospitalet (running)
0x0000 0x7856: pmt_pid 0x0510 Radio Hospitalet -- Radio Hospitalet (running)
0x0000 0x785a: pmt_pid 0x0310 Televisio Badalona -- Televisio Badalona (running)
0x0000 0x785b: pmt_pid 0x0610 Radio Ciutat Badalona -- Radio Ciutat Badal
----------------------------------------------------------------------------------------------------------------

Vous avez maintenant accès au PID PMT (en hexadecimal), vous pouvez le convertir en décimal et utiliser l'autoconfiguration partielle.

Après ces blocs vous obtiendrez des lignes comme : 

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

Vouz avez maintenant accès aux autres PIDs.

MuMuDVB a besoin des PIDs en décimal, vous avez donc a faire la conversion Hexadécimal->décimal.


Scanner seulement un transpondeur 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Vous devez d'abord accorder votre carte sur la fréquence désirée ( avec, par exemple, `tune`, `szap` ou `tzap` ).

Ensuite vous pouvez utiliser l'utilitaire scan :

----------------------
scan -o pids -c -a 0
----------------------

Où 0 doit être remplacé par le numéro de carte

Et vous obtiendrez des résultats similaires a ceux présentés dans la section <<scan_initial_tuning,utiliser scan avec un fichier d'accord initial>>



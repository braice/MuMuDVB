MuMuDVB - README
================
Brice Dubost <mumudvb@braice.net>
Version 1.6.1

[NOTE]
Une version HTML de ce fichier est disponible sur http://mumudvb.braice.net[le site web de MuMuDVB].

image::http://mumudvb.braice.net/mumudvb/logo_mumu_wiki.png[caption="logo MuMuDVB"]

Présentation
------------

Description
~~~~~~~~~~~

MuMuDVB (Multi Multicast DVB) est, à l'origine, une modification de dvbstream pour le http://www.crans.org[cr@ns]. Nous avons décidé de redistribuer le projet.

MuMuDVB est un projet indépendant.


MuMuDVB est un programme qui rediffuse des flux (chaînes de télévision) depuis une carte DVB (télévision numérique) sur un réseau en utilisant du multicast ou de l'unicast HTTP. Il peut diffuser un transpondeur (ensemble de chaînes regroupées sur une même fréquence) entier en assignant une ip multicast par chaîne.

Site Web
~~~~~~~~

http://mumudvb.braice.net[Site principal de MuMuDVB]

Auteurs et contacts
-------------------

.Auteur principal
- mailto:mumudvb@braice.net[Brice Dubost]

.Contributeurs
- mailto:manu@REMOVEMEcrans.ens-cachan.fr[Manuel Sabban] (getopt)
- mailto:glondu@REMOVEMEcrans.ens-cachan.fr[Stéphane Glondu] (man page, debian package)
- Special thanks to Dave Chapman (dvbstream author and contributor)
- Pierre Gronlier, Sébastien Raillard, Ludovic Boué, Romolo Manfredini
- Utelisys Communications B.V. pour le transcodage


.Liste de discussion
- mailto:mumudvb-dev@REMOVEMElists.crans.org[MuMuDVB dev]
- https://lists.crans.org/listinfo/mumudvb-dev[Information et inscriptions à la liste MuMuDVB dev]

[NOTE]
Lorsque vous contactez à propos d'un problème, n'oubliez pas de joindre le fichier de configuration utilisé et la sortie de MuMuDVB en mode verbeux ( -vvv sur la ligne de commande ) ajoutez y toute information qui pourrait être utile. Merci.


Fonctionnalités
----------------

Fonctionalités principales
~~~~~~~~~~~~~~~~~~~~~~~~~~

- Diffuse les chaînes d'un transpondeur vers différents groupes (adresses IP) multicast.
- MuMuDVB peux réécrire le PID PAT (table d'allocation des programmes) pour n'annoncer que les chaînes présentes (utile pour certaines set-top box). Voir la section <<pat_rewrite, réécriture du PAT>>.
- MuMuDVB peux réécrire le PID SDT (table de description des programmes) pour n'annoncer que les chaînes présentes (utile pour certains clients). Voir la section <<sdt_rewrite, réécriture du SDT>>.
- Support des chaines cryptées (si vous n'avez pas de CAM vous pouvez utiliser sasc-ng mais vérifiez que c'est autorisé dans votre pays/par votre abonnement)
- Configuration automatique, i.e. déouverte automatique des chaînes, référez-vous à la section <<autoconfiguration,Autoconfiguration>>.
- Génération des annonces SAP, voir la section <<sap, annonces SAP>>.
- Support pour DVB-S2 (satellite), DVB-S (satellite), DVB-C (cable), DVB-T (terrestre) et ATSC (terrestre ou cable en amérique du nord)
- Support pour l'unicast HTTP. voir la section <<unicast,unicast HTTP>>
- Support pour les en-têtes RTP (seulement pour la diffusion en multicast)
- Transcodage, référez vous à la section <<transcoding,Transcodage>>
- Acces au Menu cam pendant la diffustion (Utilisant une interface web/AJAX - voir link:WEBSERVICES.html[WEBSERVICES.txt])


Liste détaillée des fonctionalités
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Possibilité de montrer le niveau de réception pendant la diffusion
- Montre si les chaines sont désembrouillées avec succès par le module CAM
- Génère une liste des chaines "actives" et "inactives" en temps réel
- Peut se transformer en démon et écrire sont identifiant de processus dans un fichier
- Supporte les LNB standards et universels
- Peux s'arréter automatiquement après un certain délai si aucune donnée n'est reçue.
- Arrête d'essayer d'accorder la carte après un délai configurable
- Les PIDs obligatoire sont transmis avec toutes les chaînes : 
  * PAT (0): Program Association Table : Contient la liste des programmes et leurs PIDs PMT.
  * CAT (1): Conditionnal Access Table : Contient des informations concernant les chaînes brouillées.
  * NIT (16) : Network Information Table : Fournit des informations à propos du réseau physique.
  * SDT (17) : Service Description Table : Le pid SDT contient des données décrivant les différents services ex : nom des services, fournisseur du service, etc.
  * EIT (18) : Event Information Table : Le pid EIT contient des données concernant les programmes comme : le nom du programme, l'heure de début, la durée, une description etc ...
  * TDT (20): Time and Date Table : Le pid TDT contient l'heure et la date. Cette information est dans un pid à part car sa mise à jour est fréquente.
- Peux s'inscrire à tous les groupes multicast (IGMP membership request) pour éviter que certains switchs ne broadcastent les paquets. Se référer à la section <<problems_hp,problèmes avec certains switchs HP>>.
- Supporte l'autoconfiguration, pour plus de détails voir la section <<autoconfiguration,Autoconfiguration>>
  * En mode d'autoconfiguration, MuMuDVB suit les changements dans les PIDs des chaînes et se met a jour automatiquement.
- Scripts d'initialisation style Debian
- La taille du tampon peux être réglée pour réduire l'utilisation processeur. Voir la section <<reduce_cpu,réduire l'utilisation processseur>>
- Peux ne pas envoyer les paquets brouillés
- Détecte automatiquement si une chaîne est brouillée
- Peut réinitialiser le module CAM si l'initialisation échoue
- Peut trier le PID EIT pour envoyer seulement ceux correspondant a la chaine courante
- La lecture des données peux se faire via un thread, voir la section <<threaded_read, lecture par thread>>
- Génération de listes de lecture, voir <<playlist, playlist>>
- Possibilité d'utiliser des mots-clefs



Installation
------------

À partir des sources
~~~~~~~~~~~~~~~~~~~~


À partir d'un snapshot
^^^^^^^^^^^^^^^^^^^^^^

Si vous avez téléchargé un snapshot, vous devez regénérer les fichiers d'auto(conf make etc ...). Pour faire cela, vous avec besoin des autotools, automake, gettext et libtool et taper, dans le répertoire de MuMuDVB : 

----------------
autoreconf -i -f
----------------

Ainsi vous aurez un source qui peux être installé comme un source "relase"

À partir d'une "release"
^^^^^^^^^^^^^^^^^^^^^^^^

Pour installer MuMuDVB, tapez : 

--------------------------------------
$ ./configure [options pour configure]
$ make
# make install
--------------------------------------

Les `[options pour configure]` spécifiques à MuMuDVB sont : 

--------------------------------------------------------------------------------------------------------------
  --enable-cam-support    Support pour les modules CAM (actif par défaut)
  --enable-coverage       Compiler avec le support pour les tests de couverture du code (désactivé par défaut)
  --enable-duma           Librairie de debug DUMA (désactivé par défaut)
--------------------------------------------------------------------------------------------------------------

Vous pouvez obtenir la liste de toutes les options de configure en tapant

--------------------
$ ./configure --help
--------------------

[NOTE]
Le support pour les modules CAM dépends des librairies libdvben50221, libucsi ( des dvb-apps de linuxtv ). Le script configure détectera la présence de ces librairies et désactivera le support pour les modules CAM si l'une d'elle est absente.

[NOTE]
Le décodage des noms de chaîne longs pour l'autocoonfiguration en ATSC dépends de la librairie libucsi ( des dvb-apps de linuxtv ). Le script configure détectera la présence de cette librairies et désactivera le support pour les noms longs si elle est absente. L'autoconfiguration complète fonctionnera toujours avec l'ATSC mais les noms de chaîne seront des noms courts ( 7 caractères maximum ).

[NOTE]
Si vous voulez compiler la documentation i.e. générer des fichiers HTML avec asciidoc, tapez `make doc`. Le rendu des table fonctionnera avec asciidoc 8.4.4 ( il peux fonctionner avec des versions plus faibles mais il n'a pas été testé ).

Pour installer les scripts d'initialisation ( style debian ) tapez : 

------------------------------------------------------------
# cp scripts/debian/etc/default/mumudvb /etc/default/mumudvb
# cp scripts/debian/etc/init.d/mumudvb /etc/init.d/mumudvb
------------------------------------------------------------

[NOTE]
Il est conseillé de créer un utilisateur système pour MuMuDVB, par exemple : `_mumudvb`. Vous devez ajouter cet utilisateur au groupe vidéo et rendre le répertoire `/var/run/mumudvb` en lecture-écriture pour cet utilisateur. En faisant cela, vous pourrez profiter de toutes les fonctionnalités de MuMuDVB.


À partir du paquet Debian
~~~~~~~~~~~~~~~~~~~~~~~~~

Si vous voulez installer une version qui n'est pas dans vos dépôts, vous pouvez le faire à la main en tapant : 

----------------------
# dpkg -i mumudvb*.deb
----------------------

Sinon vous pouvez utiliser aptitude ou synaptic.

Utilisation
-----------

La documentation concernant la syntaxe du fichier de configuration est dans le fichier `doc/README_CONF-fr.txt` ( link:README_CONF-fr.html[Version HTML] ).

Utilisation:

---------------------------------------------------
mumudvb [options] -c fichier_de_configuration
mumudvb [options] --config fichier_de_configuration
---------------------------------------------------

Les différentes options sont :

--------------------------------------------------------------------------------------
-d, --debug
	Ne se met pas en tâche de fond et affiche les messages sur la sortie standard.

-s, --signal
	Affiche la force du signal toutes les 5 secondes

-t, --traffic
	Affiche toutes les 10 secondes la bande passante prise par chaque chaîne

-l, --list-cards
	Liste les cartes DVB et quitte

--card
	La carte DVB a utiliser ( le fichier de configuration est prioritaire en cas de conflit )

--server_id
	Le server id ( pour l'autoconfiguration, le fichier de configuration est prioritaire en cas de conflit )

-h, --help
	Montre l'aide ( en Anglais )

-v
	Plus verbeux ( ajouter plusieurs v pour plus de verbosité )

-q
	Plus silencieux ( ajouter pour rendre encore plus silencieux )

--dumpfile
	Option de debug : Sauvegarde le flux reçu dans le fichier spécifié

--------------------------------------------------------------------------------------

Signal: ( voir kill(1) )
------------------------------------------------------------------
    SIGUSR1: bascule l'affichage de la force du signal
    SIGUSR2: bascule l'affichage du traffic
    SIGHUP: force l'écriture des fichiers de log
------------------------------------------------------------------

[[autoconfiguration]]
Autoconfiguration
-----------------

MuMuDVB est capable de trouver les différentes chaînes dans le transpondeur et leur PIDs ( Program IDentifiers ).

Sans l'autoconfiguration, vous devez spécifier les paramètres du transpondeur et, pour chaque chaîne, l'ip de multicast, le nom et les PIDs ( PMT, audio, vidéo, télétexte etc... ).

À la fin de l'autoconfiguration MuMuDVB génère un fichier de configuration avec les paramètres obtenus. Le fichier généré est, par défaut : `/var/run/mumudvb/mumudvb_generated_conf_card%d_tuner%d`

Si les PIDs sont changés, MuMuDVB mettra automatiquement à jour les chaînes sauf si vous mettez `autoconf_pid_update=0` dans votre fichier de configuration.

MuMuDVB est capable de s'autoconfigurer de deux manières différentes : 

Autoconfiguration "complète"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

C'est la manière la plus simple pour utiliser MuMuDVB.

Utilisez ce mode lorsque vous voulez diffuser un transpondeur entier ou une partie d'un transpondeur (en utilisant autoconf_sid_list).

[NOTE]
Vous ne devez pas spécifier de chaînes en utilisant ce mode.

Dans ce mode, MuMuDVB trouvera pour vous les différentes chaînes, leurs noms et leurs PIDs (PMT, PCR, Audio, Vidéo, Sous-titres, Télétexte et AC3).

Pour utiliser ce mode, vous devez :
- Définir les paramètres d'accord dans votre fichier de configuration
- Ajouter `autoconfiguration=full` à votre fichier de configuration
- Vous n'avez a spécifier aucune chaîne
- Pour une première utilisation, n'oubliez pas d'ajouter la paramètre `-d` lorsque vous lancez MuMuDVB :
   ex `mumudvb -d -c your_config_file`

.Example Fichier de configuration pour une réception satellite à une fréquence de 11.296GHz et une polarisation horizontale
--------------------
freq=11296
pol=h
srate=27500
autoconfiguration=full
--------------------

Les chaînes seront diffusées sur les adresses ip de multicast 239.100.c.n où c est le numéro de carte ( 0 par défaut ) et n est le numéro de chaîne.

Si vous n'utilisez pas l'option common_port, MuMuDVB utilisera le port 1234.

[NOTE]
Par défaut, les annnonces SAP sont activées si vous utilisez ce mode d'autoconfiguration. Pour les désactiver, ajoutez `sap=0` dans votre fichier de configuration.
Par défaut, la réécriture du PID SDT est activée si vous utilisez ce mode d'autoconfiguration. Pour la désactiver, ajoutez `rewrite_sdt=0` dans votre fichier de configuration.
Par défaut, la réécriture du PID PAT est activée si vous utilisez ce mode d'autoconfiguration. Pour la désactiver, ajoutez `rewrite_pat=0` dans votre fichier de configuration.
Par défaut, le tri du PID EITT est activée si vous utilisez ce mode d'autoconfiguration. Pour le désactiver, ajoutez `sort_eit=0` dans votre fichier de configuration.

[NOTE]
Un fichier de configuration d'exemple, documenté (en anglais) est disponible : `doc/configuration_examples/autoconf_full.conf`

Modèles de nom et autoconfiguration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Par défaut le nom de la chaîne sera le nom du service définit par le diffuseur. Si vous voulez plus de flexibilité vous pouvez utiliser un modèle.

Par exemple, si vous utilisez `autoconf_name_template=%number-%name` Les noms de chaîne auront la forme suivante : 

- `1-CNN`
- `2-Euronews`

Il y a différents mot-clef disponibles

[width="80%",cols="2,8",options="header"]
|==================================================================================================================
|Mot clef |Description 
|%name | Le nom donné par le diffuseur
|%number | Le numéro de chaîne de MuMuDVB 
|%card | Le numéro de carte DVB
|%tuner | Le numéro de tuner
|%server| Le numéro de serveur, spécifié en ligne de commande ou via l'option server_id
|%lcn | Le "logical channel number" ( numéro de chaîne attribué par le diffuseur ). Vous devez mettre `autoconf_lcn=1` dans votre fichier de configuration et votre diffuseur doit diffuser le LCN. Le LCN sera "affiché" avec trois chiffre incluant des 0. Ex "002". Si le LCN n'est pas détecté, %lcn sera remplacé par une chaîne de caractères vide
|%2lcn | Idem que précédemment mais avec un format à deux chiffres.
|==================================================================================================================

Reférez vous au README_CONF pour savoir quelles options acceptent quels mot clefs

D'autres mot clefs peuvent être facilement ajoutés si nécessaire.



[[autoconfiguration_simple]]
Autoconfiguration "simple"
~~~~~~~~~~~~~~~~~~~~~~~~~~

[NOTE]
Ce mode d'autoconfiguration disparaîtra bientôt. Si vous en avez absolument besoin pour autre chose que spécifier le nom ou l'IP merci de me contacter.

Utilisez ce mode lorsque vous voulez contrôler le nom des chaînes, leurs IP d'une manière plus approfondie qu'avec les templates.

- Vous devez ajouter 'autoconfiguration=partial' dans le début de votre fichier de configuration.
- Pour chaque chaîne, vous devez préciser : 
 * L'adresse IP ( si vous n'utilisez pas l'unicast )
 * Le nom
 * Le PID PMT


MuMuDVB trouvera les PIDs audio, vidéo, PCR, télétexte, sous-titres et AC3 avant de diffuser.


[NOTE]
Si vous mettez plus d'un PID pour une chaîne, MuMuDVB désactivera l'autoconfiguration pour cette chaîne.

[NOTE]
Un fichier de configuration détaillé et documenté (en anglais) se trouve à l'emplacement : `doc/configuration_examples/autoconf1.conf`

[NOTE]
L'autoconfiguration simple peut ne pas réussir à trouver les bon PIDs si le PID PMT est partagé entre plusieurs chaînes. Dans ce cas, vous devez spécifier le numéro de programme ( service identifier ou stream identifier ) via l'option `service_id`.


[[sap]]
Annonces SAP
------------

Les annonces SAP (Session Announcement Protocol) ont été conçues pour que le client obtienne automatiquement la liste des chaînes diffusées et leurs adresses. Ceci permet d'éviter d'avoir a donner la liste des adresses ip de multicast a chaque client. 

VLC et la plupart des set-top boxes supportent les annonces SAP.

MuMuDVB générera et enverra les annonces SAP si cela est demandé via le fichier de configuration ou en mode d'autoconfiguration complète.

Les annonces SAP seront envoyées uniquement pour les chaînes actives. Quand une chaîne devient inactive, MuMuDVB arrête automatiquement d'envoyer les annonces pour cette chaîne jusqu'à ce qu'elle redevienne active.


Demander à MuMuDVB de générer les annonces SAP
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Pour envoyer les annonces SAP, vous devez ajouter `sap=1` à votre fichier de configuration. Les autres paramètres concernant les annonces SAP sont documentés dans le fichier `doc/README_CONF-fr` ( link:README_CONF-fr.html[Version HTML] ).

Annonces SAP et autoconfiguration complète
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Si vous utilisez l'autoconfiguration complète, vous pouvez utiliser le mot-clef '%type' dans l'option sap_default_group. Ce mot clef sera remplacé par le type de la chaîne : Television ou Radio.

.Example
Si vous mettez `sap_default_group=%type`, vous obtiendrez deux groupes SAP : Television et Radio, chacun contenant les services coresspondants.


Configurer le client pour recevoir les annonces SAP
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

VLC > 0.8.2
^^^^^^^^^^^
Vous devez aller dans paramètres, puis paramètres avancés. Les annonces SAP peuvent êtres activés dans liste de lecture -> découverte de services.

N'oubliez pas de sauvegarder les paramètres.

Vous devriez avoir désormais une section SAP dans votre liste de lecture.

VLC < 0.8.2
^^^^^^^^^^^

Allez dans le menu `paramètres` puis `ajouter une interface` et choisissez `liste de lecture SAP`, Désormais, lorsque vous ouvrez la liste de lecture, les annonces SAP devraient apparaître automatiquement.

[[unicast]]
Unicast HTTP
------------

En plus du multicast, MuMuDVB supporte aussi l'unicast HTTP. Cela vous permet d'utiliser MuMuDVB à travers des réseau qui ne supportent pas le multicast.

Il y a une connexion en écoute, la chaîne est sélectionnée par le chemin HTTP, voir plus loin.

[NOTE]
Rappelez vous que l'unicast peut consommer beaucoup de bande passante. Pensez à limiter le nombre de clients.

[NOTE]
Si vous ne voulez pas que le flux multicast (toujours présent) aille sur votre réseau, utilisez l'option `multicast=0`

Activer l'unicast HTTP
~~~~~~~~~~~~~~~~~~~~~~

Pour activer l'unicast HTTP vous devez définir l'option `unicast`. Par défaut, MuMuDVB écoute sur toutes vos interfaces.

Vous pouvez aussi définir le port d'écoute en utilisant l'option `port_http`. Si le port n'est pas défini, MuMuDVB utilisera le port par défaut : 4242.

Activer l'unicast par chaîne
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Vous pouvez créer des connections en écoute seulement pour une chaîne. Dans ce cas, quand un client se connecte dessus, il obtiendra toujours la même chaîne indépendamment du chemin HTTP.

Vous utilisez l'autoconfiguration complète
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Vous devez définir l'option `autoconf_unicast_start_port` qui précise quel sera le port d'écoute pour la première chaîne découverte ( le port sera incrémenté pour chaque chaîne ).


Vous n'utilisez pas l'autoconfiguration complète
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Pour les chaînes pour lesquelles avoir une connexion en écoute, vous devez définir l'option `unicast_port` qui permet de spécifier le port d'écoute.


Coté client, les différentes méthodes pour obtenir les chaînes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[[playlist]]
Utilisation d'une playlist
^^^^^^^^^^^^^^^^^^^^^^^^^^

MuMuDVB génère des playlist au format m3u. 

Si votre serveur écoute sur l'ip 10.0.0.1 et le port 4242,

-------------------------------------
vlc http://10.0.0.1:4242/playlist.m3u
-------------------------------------

[NOTE]
Dans la playlist, les chaînes seront annoncées avec des URL du type `/bysid/` ( voir plus bas ), si vous voulez des URLs de chaîne par port utilisez l'URL `/playlist_port.m3u`.

[NOTE]
Des playlists pour la lecture multicast sont aussi générées, elles sont accessibles aux addresses suivantes : "playlist_multicast.m3u" et "playlist_multicast_vlc.m3u"

Cas d'une connexion pour une chaîne
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Si le client se connecte au serveur sur un port qui est pour une seule chaîne, il obtiendra cette chaîne indépendamment du chemin demandé

Si votre serveur écoute sur l'ip 10.0.0.1 et le port pour la chaîne est 5000,

-------------------------
vlc http://10.0.0.1:5000/
-------------------------

Obtenir les chaînes par numéro
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Vous pouvez obtenir les chaînes en utilisant leur numéro ( la première chaine est numéroté 1 ).

Si votre serveur écoute sur l'ip 10.0.0.1 et le port 4242,

------------------------------------
vlc http://10.0.0.1:4242/bynumber/3
------------------------------------

vous donnera la chaîne numéro 3. Ceci marche aussi avec xine et mplayer.

Obtenir les chaînes par leur service id
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Vous pouvez obtenir les chaînes en utilisant leur service id.

Si votre serveur écoute sur l'ip 10.0.0.1 et le port 4242,

------------------------------------
vlc http://10.0.0.1:4242/bysid/100
------------------------------------

vous donnera la chaîne numéro avec le service id 100 (ou une erreur 404 si il n'y a pas de chaine avec ce service id). Ceci marche aussi avec xine et mplayer.

Obtenir les chaînes par leur nom
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

[NOTE]
Cette fonctionnalité n'est pas implémentée pour le moment. Elle sera implémentée dans une prochaine version.

Obtenir la liste des chaînes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Si votre serveur écoute sur l'ip 10.0.0.1 et le port 4242,

Pour obtenir la liste des chaînes ( format HTML "basique" ) entrez l'adresse `http://10.0.0.1:4242/channels_list.html` dans votre navigateur.

Pour obtenir la liste des chaînes ( format JSON ) entrez l'adresse `http://10.0.0.1:4242/channels_list.json` dans votre navigateur.

Unicast HTTP et surveillance de MuMuDVB
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Cette connexion HTTP peut être utilisée pour surveiller MuMuDVB.

Des informations de surveillance sont disponibles au format JSON ( http://fr.wikipedia.org/wiki/JSON ) via les URLs suivantes : `/monitor/signal_power.json` et `/monitor/channels_traffic.json`

Il est assez aisé d'ajouter de nouvelles informations si nécessaire.


Surveillance de MuMuDVB
-----------------------

Vous pouvez utiliser http://mmonit.com/monit/[Monit] pour surveiller MuMuDVB et le redémarrer lorsqu'il a des problèmes (MuMuDVB se suicide lorsque de gros problèmes apparaissent).

Vous devez installer les scripts de démarrage ( fait automatiquement si vous utilisez le paquet Debian ) et ajouter les lignes suivantes à votre fichier `/etc/monit/services` :

----------------------------------------------------------------------
check process mumudvb with pidfile /var/run/mumudvb/mumudvb_adapter0_tuner0.pid
    start program = "/etc/init.d/mumudvb start"
    stop program = "/etc/init.d/mumudvb stop"
----------------------------------------------------------------------

[NOTE]
Le 0 doit être remplacé par le numéro de votre carte si vous avez plusieurs cartes.

Pour plus d'informations référez vous au http://mmonit.com/monit/[site web de Monit].

MuMuDVB tourne habituellement pendant des jours sans le moindre problème, mais avec monit vous êtes sur. Monit est aussi capable d'envoyer des e-mails en cas de problèmes.


Support pour les chaînes brouillées
-----------------------------------

Note importante : vérifiez si le contrat avec votre fournisseur de services vous autorise à diffuser les chaînes brouillées.

Débrouillage matériel
~~~~~~~~~~~~~~~~~~~~~

MuMuDVB supporte les chaînes brouillées à l'aide d'un décodeur matériel ( un module CAM : Conditionnal Access Module ). MuMuDVB peut demander au module de débrouiller plusieurs chaînes si le module le supporte ( les CAMs Aston Pro ou PowerCam Pro sont capable de débrouiller plusieurs chaînes simultanément).

Si vous êtes limités par le nombre de PIDs que le module CAM peut débrouiller simultanément, il est possible de demander au module de débrouiller seulement les PIDs audio et vidéo. Cette fonctionnalité n'est pas implémentée, merci de contacter si vous en avez besoin.

[NOTE]
Le débrouillage matériel n'utilise quasiment aucune puissance processeur, tout le débrouillage est effectué par le module CAM.

[NOTE]
MuMuDVB ne demande pas au module CAM si le débrouillage est possible. Cette fonctionnalité des modules CAM n'est pas fiable. La plupart des modules CAM répondront par un menu si le débrouillage n'est pas possible et MuMuDVB l'affichera sur la sortie standard d'erreur.

Les informations concernant le module CAM sont stockées dans le fichier '''/var/run/mumudvb/caminfo_adapter%d_tuner%d''' où %d est le numéro de carte DVB.

.Example 
----------------------------------------------------
CAM_Application_Type=01
CAM_Application_Manufacturer=02ca
CAM_Manufacturer_Code=3000
CAM_Menu_String=PowerCam_HD V2.0
ID_CA_Supported=0100
ID_CA_Supported=0500
----------------------------------------------------


Comment demander le débrouillage ?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.Vous utilisez le mode d'autoconfiguration complet :

Il vous suffit d'ajouter `cam_support=1` à votre fichier de configuration.

.Vous utilisez l'autoconfiguration partielle ou pas d'autoconfiguration 
 * Ajoutez `cam_support=1` à votre fichier de configuration ( avant la définition des chaînes )
 * Pour chaque chaîne brouillée, ajoutez l'option `cam_pmt_pid`. Cette option permet à MuMuDVB de savoir quel PID est le PID PMT. Ce PID sera utilisé pour demander le débrouillage.


[NOTE]
Vous avez un exemple de fichier de configuration avec le support pour les chaînes brouillées dans `doc/configuration_examples/autoconf1.conf`


Débrouillage logiciel
~~~~~~~~~~~~~~~~~~~~~

Note importante : Cette solution n'est pas autorisée par certains contrats de fournisseurs de services.

MuMuDVB a été testé avec succès avec des solutions de débrouillage logiciel comme sascng + newcs + dvbloopback.

Dans ce cas, vous n'avez pas besoin de définir l'option `cam_support`. Vous devez juste ajuste l'option `card` pour être en accord avec votre carte virtuelle dvbloopback.

Si vous utilisez ces solutions référez vous à la section <<reduce_cpu,réduire l'utilisation CPU de MuMuDVB>>.

État du débrouillage
~~~~~~~~~~~~~~~~~~~~

L'état du débrouillage est stocké avec la liste des chaînes diffusées. 

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

 * FullyUnscrambled : Moins de 5% de paquets brouillés
 * PartiallyUnscrambled : entre 5% et 95% de paquets brouillés
 * HighlyScrambled : plus de 95% de paquets brouillés

[[pat_rewrite]]
Réécriture du PID PAT (Program Allocation Table)
------------------------------------------------

Cette fonctionnalité est principalement destinée pour les set-top boxes. Cette option permet d'annoncer uniquement la chaîne diffusée dans le PID PAT (Program Allocation Table) au lieu de toutes les chaînes du transpondeur. Les clients sur ordinateur regardent cette table et décode le premier programme avec des PIDs audio/vidéo. Les set-top boxes décodent habituellement le premier programme de cette table ce qui résulte en un écran noir pour la plupart des chaînes.

Pour activer la réécriture du PAT, ajoutez `rewrite_pat=1` à votre fichier de configuration. Cette fonctionnalité utilise peu de puissance processeur, la table PAT étant réécrite une fois par chaîne et stockée en mémoire.

[NOTE]
La réécriture du PAT peu échouer (i.e. ne résout pas les symptômes précédents) pour certaines chaînes si le PID PMT est partagé par plusieurs chaînes. Dans ce cas, vous devez ajouter l'option `service_id` pour spécifier le "service id" ou numéro de programme.

[[sdt_rewrite]]
Réécriture du PID SDT (Service Description Table)
-------------------------------------------------

Cette option permet d'annoncer uniquement la chaîne diffusée dans le PID SDT (Service Description Table) au lieu de toutes les chaînes du transpondeur. Certains clients regardent cette table et peuvent ainsi montrer/sélectionner  des programmes fantomes si cette table n'est pas réécrite (même si le PAT est réécrit). Ceci peut se manifester par un écran noir de manière aléatoire.

Pour activer la réécriture du SDT, ajoutez `rewrite_sdt=1` à votre fichier de configuration. Cette fonctionnalité utilise peu de puissance processeur, la table SDT étant réécrite une fois par chaîne et stockée en mémoire.

[NOTE]
Si vous n'utilisez pas l'autoconfiguration complète, la réécriture du SDT nécessite l'option `service_id` pour spécifier le "service id" ou numéro de programme.

Tri du PID EIT (Event Information Table)
----------------------------------------

Cette option permet de ne diffuser que les packets EIT (Event Information Table) correspondant à la chaîne diffusée au lieu de toutes les chaînes du transpondeur. Certains clients regardent cette table et peuvent ainsi montrer/sélectionner des programmes fantomes si cette table n'est pas réécrite (même si le PAT et le SDT sont réécrits).

Pour activer le tri du PID EIT, ajoutez `sort_eit=1` à votre fichier de configuration.

[NOTE]
Si vous n'utilisez pas l'autoconfiguration complète, le tri du PID EIT nécessite l'option `service_id` pour spécifier le "service id" ou numéro de programme.


[[reduce_cpu]]
Réduire l'utilisation processeur
--------------------------------

Normalement MuMuDVB lit les paquets de la carte un par un et demande à la carte après chaque lecture si il y a un nouveau paquet disponible ( poll ). Mais souvent les cartes on un tampon interne ( la carte a plusieurs paquets de disponible d'un coup ) ce qui rends certains "polls" inutiles. Ces "polls" consomme du temps processeur.

Pour réduire l'utilisation CPU, une solution est d'essayer de lire plusieurs paquets à la fois. Pour faire cela, utilisez l'option `dvr_buffer_size`.

.Example
------------------
dvr_buffer_size=40
------------------

Pour voir si la valeur que vous avez choisie est trop grande ou trop basse, exécutez MuMuDVB en mode verbeux, le nombre moyen de paquets reçus en même temps sera affiché toutes les deux minutes. Si ce nombre est plus faible que votre taille de tampon il est inutile de l'augmenter.

La réduction de l'utilisation processeur peut être entre 20 et 50%.

[[threaded_read]]
Lecture des données via un thread
---------------------------------

Pour rendre MuMuDVB plus robuste ( au prix d'un légère augmentation de l'utiliation CPU ), MuMuDVB peux lire les données en provenance de la carte via un thread, ce qui rends la lecture "indépendante" du reste du programme.

Pour activer cette fonctionalitée, utilisez l'option `dvr_thread`.

Cette lecture utilise deux tampons : un pour les données reçues par la carte, un pour les données actuellement traitées par le programme principal. Vous pouvez ajuster la taille de ces tampons en utilisant l'option `dvr_thread_buffer_size`. La valeur par défaut ( 5000 packets de 188 octets ) devrait suffire pour la plupart des cas. 

Le message "Thread trowing dvb packets" vous informe que le thread recoit plus de paquets que la taille du tampon et est obligé d'en "jeter". Augmenter la taille du tampon résoudra probablement le problème. 

[[transcoding]]
Transcoding
-----------

MuMuDVB peux transcoder le flux dans différents formats pour économiser de la bande passante. Le transcodage est effectué en utilisant les librairies du projet ffmpeg. Cette fonctionnalité est assez nouvelle, n'hesitez pas à envoyer vos remarques/suggestions.

[NOTE]
Le transcodage ne fonctionne pas pour le moment avec l'unicast.


Compiler MuMuDVB avec le support pour le transcodage
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Pour compiler MuMuDVB avec le support pour le transcodage, vous aurez besoin des librairies suivantes : libavcodec, libavformat et libswscale

Vous devez aussi ajouter l'option --enable-transcoding à votre configure. Ex: `./configure --enable-transcoding`

Vérifiez à la fin du configure que le transcoding est effectivement activé. Si ce n'est pas le cas, une librairie est probablement manquante.


Utiliser le transcodage
~~~~~~~~~~~~~~~~~~~~~~~

Merci de lire la documentation concernant les options de transcodage et les exemples.

Futurs développements concernant le transcodage
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Faire fonctionner le transcodage avec l'unicast, en particulier une fois le support RTSP achevé.

Problèmes courants
~~~~~~~~~~~~~~~~~~

Broken ffmpeg default settings detected
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Si vous obtenez un messaage disant : "Broken ffmpeg default settings detected"

Ajouter les options suivante a votre configuration de transcodage lui permettra de fonctionner : 

------------------------------
transcode_me_range=16
transcode_qdiff=4
transcode_qmin=10
transcode_qmax=51
------------------------------


Codec not found
^^^^^^^^^^^^^^^

Si le module de transodage ne trouve pas le codec que vous voulez, vous devez probablement installer des codecs supplementaires (libavcodec-extra-52 pour debian).

Lancez MuMuDVB en mode verbeux vous donnera la liste des codecs disponible.


Le flux non transcodé est envoyé
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Pour eviter aux flux original d'être envoyé, vous pouvez utiliser l'option transcode_send_transcoded_only.



[[ipv6]]
IPv6
----

MuMuDVB supporte le multicast IPv6. Il n'est pas activé par défaut. Vous devez l'activer avec l'option multicast_ipv6.

Pour "apprécier" le multicast IPv6 vous devez utiliser des switch supportant le protocole http://en.wikipedia.org/wiki/Multicast_Listener_Discovery[Multicast Listener Discovery].

IPv6 utilise intensivement le concept de [http://en.wikipedia.org/wiki/Multicast_address]scope. Par défaut, MuMuDVB utilise le scope "site-local" (càd des addresses multicast commencant par FF05) les annonces SAP sont aussi envoyées avec ce scope. Si vous avez besoin de plus de flexibilité de ce coté merci de contacter.

Pour plus de détails consultez http://mumudvb.braice.net/mumudrupal/node/52[la page sur l'IPv6] sur le site de MuMuDVB.



Détails techniques (en vrac)
----------------------------

 * Consommation processeur : MuMuDVB utilise 15% du temps processeur d'un Celeron 2.6GHz avec une carte satellite Hauppauge et un noyau 2.6.9 lorsqu'un transpondeur entier est diffusé ( environs 30Mbit/s)

 * Essayez d'éviter les vieux chipset via ou nForce et, de manière générale les cartes mères à très bas coût. Ils ne supportent pas d'avoir un important flux de données sur le bus PCI.

 * Lorsque le programme démarre, il écrit la liste des chaînes dans le fichier `/var/run/mumudvb/channels_streamed_adapter%d_tuner%d` ( où %d sont le numéro de carte et le numéro de tuner ). Ce fichier contient la liste des chaînes diffusées ( mise à jour toutes les 5 secondes ) sous la forme : "nom:ip:port:EtatDuBrouillage"

 * MuMuDVB peut supporter autant de cartes que le système d'exploitation. Les anciennes version de udev+glibc ne supportaient pas plus de 4 cartes mais ce problème est résolu en utilisant des versions relativement récentes ( udev > 104 et libc6 > 2.7 )

 * Lorsque MuMuDVB tourne en démon, il écrit son identifiant de processus dans le fichier `/var/run/mumudvb/mumudvb_adapter%d_tuner%d.pid`, où %d est remplacé par le numéro de carte et de tuner.

 * MuMuDVB supporte la réception satellite en bande Ku avec des LNB universel ou standard. Le support pour la réception satellite en bande S ou C est implémenté via l'utilisation de l'option `lo_frequency`. Référez vous au fichier `doc/README_CONF-fr` ( link:README_CONF-fr.html[Version HTML] ) pour plus de détails.



MuMuDVB Logs
------------

MuMuDVB peux envoyer ses logs sur la console, dans un fichier ou via syslog. Il peux aussi les envoyer dans plusieurs de ces canaux. Le formattage des messages peut aussi etre ajusté.

Par défaut, les logs sont envoyé sur la console si MuMuDVB ne tourne pas en démon et via syslog sinon.

Si les logs sont envoyés dans un fichier, vous pouvez forcer l'écriture (flush) en envoyant le signal SIGHUP au processus MuMuDVB.

Pour plus d'informations référez vous au fichier `doc/README_CONF-fr` ( link:README_CONF-fr.html[Version HTML] ).


Problèmes connus
----------------

VLC n'arrive pas à lire le flux mais cela marche avec xine ou mplayer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 * Pour VLC vous devez spécifier le PID PMT en plus des PIDs audio et vidéo.
C'est un problème fréquent. Pour le résoudre, vous pouvez utilisez le mode verbeux de VLC ( `vlc -v` ) et vous obtiendrez une ligne comme : `[00000269] ts demuxer debug:   * number=1025 pid=110`. Vous obtiendrez le PID PMT associé à votre numéro de programme. Vous pouvez aussi utiliser dvbsnoop, ou vous référer à la section "comment obtenir les PIDs" du fichier `doc/README_CONF-fr` ( link:README_CONF-fr.html[Version HTML] ). Une autre solution est d'utiliser l'autoconfiguration complète.

VLC arrive a lire la vidéo mais pas le son
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 * Ce problème peut arriver lorsque le PID PCR ( i.e. l'horloge ) n'est pas le PID vidéo. Dans ce cas, vous devez vérifier si le PID PCR est bien dans votre liste de PIDs.

MuMuDVB ne marche pas en mode démon
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 * Pour passer en mode démon, MuMuDVB a besoin que le répertoire `/var/run/mumudvb/` soit autorisé en écriture pour pouvoir écrire son identifiant de processus et la liste des chaînes.

Le système plante ou gèle
~~~~~~~~~~~~~~~~~~~~~~~~~

 * Les vieux chipset VIA ou nForce sont des chipset grand public. Ils ne peuvent pas traiter une quantité importante de données sur le bus PCI. Mais vous pouvez tenter d'ajuster les paramètres de votre BIOS.

Problèmes d'accord en DVB-T
~~~~~~~~~~~~~~~~~~~~~~~~~~~

 * Vous devez vérifier les paramètres d'accord, sachant que la détection automatique de la bande passante ne marche pas généralement.

Ma set top box affiche un écran noir
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 * Si le flux fonctionne bien lorsqu'il est lu via un ordinateur et pas sur votre set-top box, c'est probablement que votre set-top box a besoin que le PID PAT soit réécrit. MuMuDVB peut réécrire le PID PAT, pour cela ajoutez `rewrite_pat=1` à votre fichier de configuration.

L'autoconfiguration partielle ne trouve pas les bon PIDs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 * Ceci peut arriver lorsqu'un PID PMT est partagé entre plusieurs chaînes. Référez vous à la section <<autoconfiguration_simple,autoconfiguration partielle>> pour plus de détails.


Le module CAM refuse de décrypter des chaînes "locked"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 * Certains modules CAM viaccess peuvent avoir un verrou pour les chaînes "adultes". Pour enlever ce verrou, allez dans le menu du module CAM, en utilisant, par exemple, "gnutv -cammenu" (des dvb-apps de linuxtv).

Vous devez mettre le "maturity rating" au maximum et déverrouiller "Maturity rating" dans le sous-menu "Bolts".

VLC ne choisit pas le bon programme même avec la réécriture du PAT
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Vous devez aussi réécrire le PID SDT en utilisant l'option `rewrite_sdt`


[[problems_hp]]
À L'AIDE ! mon traffic multicast inonde le réseau (J'utilise un commutateur HP procurve)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

La meilleure explication est fournie dans le guide de routage du multicast de HP.


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

En conclusion, si vous avez un commutateur de la deuxième liste, c'est "normal". La solution est de faire en sorte que MuMuDVB joigne le groupe multicast. Pour cela, ajoutez `multicast_auto_join=1` dans votre fichier de configuration.

MuMuDVB utilise beaucoup de temps processeur avec sasc-ng !
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Si vous utilisez sasc-ng + dvbloopback, MuMuDVB consommera plus de temps processeur que nécessaire.

Une partie de ce temps CPU est utilisé pour débrouiller les chaînes, une autre part est due à la manière dont dvbloopback est implémenté et à la manière dont MuMuDVB interroge la carte.

Pour réduire l'utilisation processeur référez vous à la section <<reduce_cpu,réduire l'utilisation processeur de MuMuDVB>>. Dans le cas de l'utilisation de MuMuDVB avec sasc-ng cette amélioration peut être importante.

La réception est bonne mais MuMuDVB indique toutes les chaînes "down"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Si le signal est bon mais MuMuDVB vous indique que toutes les chaînes sont "down", cela peut être dû à votre module CAM si vous en possédez un. Essayez après avoir retiré votre module

Je veux diffuser à partir de plusieurs cartes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

La solution est simple : lancez un processus de MuMuDVB pour chaque carte.

Je veux diffuser tout le transpondeur sur une "chaîne"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

MuMuDVB peut diffuser toutes les données reçues par la carte sur une "chaîne" (multicast ou unicast). Pour cela, vous devez ajouter le PID 8192 dans la liste des PIDs de la chaîne.


J'ai plusieurs interfaces réseau et je veux choisir sur laquelle le traffic multicast sera envoyé
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Pour spécifier l'interface, vous povez définir une route pour le traffic multicast comme : 

---------------------------------------------------
route add -net 224.0.0.0 netmask 240.0.0.0 dev eth2
---------------------------------------------------

ou vous pouvez utiliser les options multicast_iface4 et multicast_iface6


Que veulent dire les codes d'erreur de MuMuDVB ?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Voici une courte description des codes d'erreur

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


J'obtiens le message "DVR Read Error: Value too large for defined data type"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Ce message indique qu'un débordement de tampon a eu lieu dans le tampon du pilote de la carte DVB. C'est à dire que MuMuDVB n'a pas été capable de récupérer les paquets assez vite. Ce problème peut avoir des causes
variées. Tout ce qui ralenti (beaucoup) MuMuDVB peut être la source de ce problème. Pour l'éviter vous pouvez essayer <<threaded_read, la lecture par thread>>.

Des problèmes réseau peuvent provoquer ce message :

I experienced the "DVR Read Error..." message very often on my  Streaming Server (ia64 Madison 1.3Ghz) (with errors in the video).
I could solve the problem by exchanging the network switch. The old  switch was limiting multicast traffic to 10Mb/s per port. This limit  is not documented.

I have tested the limit the programm dd and mnc (Multicast netcat,  http://code.google.com/p/mnc/)

dd if=/dev/zero bs=188 count=1000000 | ./mnc-bin 239.10.0.3

I looked with "iftop" at the current network statistics and with my  old switch i saw the limit at 10Mb/s with another switch I was able to  transmit 92Mb/s ~ 100% of the avaiable bandwith.

Merci à Jan-Philipp Hülshoff


Utilisation avec des clients "particuliers"
-------------------------------------------

Cette section n'est pas traduite, referez vous a la version anglaise.






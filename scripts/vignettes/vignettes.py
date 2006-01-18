#! /usr/bin/env python
# -*- coding: iso-8859-15 -*-

import os
import lock
import time
from time import localtime,sleep

t = localtime()
trame_entete="""
<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">
<html lang=\"fr\">
<head>
<title>Chaines diffus&eacute;es</title>
<meta http-equiv=\"refresh\" content=\"300\">
<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=iso-8859-1\">
</head>
<body BACKGROUND=\"style2_left.png\" leftmargin=\"0\" topmargin=\"0\" marginwidth=\"0\" marginheight=\"0\" STYLE=\"background-repeat:repeat-y\">
<img src=\"style2_top.png\" alt=\"En_tete\"><br>
<div id=\"Titre\" style=\"position:absolute; left:580px; top:70px; width:400px;height:34px; z-index:2\"><font size=\"5\"><b>Chaines diffus&eacute;es</b></font></div>
<div id=\"Heure\" style=\"position:absolute; left:380px; top:140px; width:500px;height:34px; z-index:2\"><font size=\"4\">
<u>Cette page &agrave; &eacute;t&eacute; g&eacute;n&eacute;r&eacute;e &agrave; %02ih%02i</u></font></div>
<div id=\"Texte\" style=\"position:absolute; left:245px; top:190px; right:16px; z-index:1; overflow: visible; visibility: visible; background-color: #FFFFFF; layer-background-color: #FFFFFF;\">
</center>
<h2><b><a href=http://wiki.crans.org/moin.cgi/TvReseau>Pour plus d'informations cliquez ici</a><br></b></h2>
<a href=index_petites.html>Petites vignettes</a><br>
<a href=index.html>Vignettes Normales</a><br>
<table cellpading=25 cellspacing=25>
<tr>
""" % (int(t[3]) ,int(t[4]) )

table_piedpage="""</tr>
</table>
"""

trame_piedpage="""</body>
</html>
"""

def ajoute_image(nom,ip,html,html_petites):
    html.write('<td align="center">')
    html.write('<img src="images/%s.jpg" alt="Chaine : %s / IP : %s"><br>' % (ip, nom, ip))
    html.write('<b><u>Chaine :</u></b> %s<br><b><u>IP :</u></b> %s:1234' % (nom, ip))
    html.write('</td>\n')
    html_petites.write('<td align="center">')
    html_petites.write('<img src="images/%s_petites.jpg" alt="Chaine : %s / IP : %s"><br>' % (ip, nom, ip))
    html_petites.write('<b><u>Chaine :</u></b> %s<br><b><u>IP :</u></b> %s:1234' % (nom, ip))
    html_petites.write('</td>\n')



def vignettes() :
    html = open('/tmp/index.html','w')
    html.write(trame_entete)
    html_petites = open('/tmp/index_petites.html','w')
    html_petites.write(trame_entete)
    col = 0
    col_petites = 0
    #a factoriser
    chaines_probleamatiques=[]
    os.system('/usr/local/bin/recup_sap')
    os.system('sort /tmp/chaines_recup_sap.txt> /tmp/chaines_recup_sap_triees.txt')
    data = open('/tmp/chaines_recup_sap_triees.txt','r').readlines()
#    for line in data :
	
#	line = line.strip()
#	nom=line.split(':')[0]
#	ip=line.split(':')[1]
	#on évite les radios, peu de debit et pas de vignettes
#	if nom[0:3]=='rad' :
#            continue
#	print 'on s\'occupe de  %s %s' % (ip,nom)
#	print '\trecuperation du flux'
        #ip port duree
#	os.system('dumpudp %s 1234 2 > /tmp/%s.ts 2>/dev/null &' % (ip,ip))
        #on dort de la durre du dump divise par le nb de flux qu'on accepte
#        sleep(1.5)

    #on attends que tout le monde aie fini
#    print '\nFin de la recuperation des flux'
#    sleep(2)

    for line in data :
	
	line = line.strip()
	nom=line.split(':')[0]
	ip=line.split(':')[1]
	#on évite les radios, peu de debit et pas de vignettes
	if nom[0:3]=='rad' :
            #C'est une radio on cherche un logo
            ok=0 #désolé vince j'ai essayé les exceptions mois c pas si simple
            nom_court=(' '.join(nom.split(' ')[2:])).lower()
            print "On s'occupe de la radio %s" % nom_court
            for file in os.listdir('/var/www/images/logos_radios'):
                if file.startswith(nom_court):
                    os.system('cp /var/www/images/logos_radios/\"%s\" /var/www/images/%s.jpg' % (file,ip))
                    os.system('cp /var/www/images/logos_radios/\"%s\" /var/www/images/%s_petites.jpg' % (file,ip))
                    if col == 2 :
                        col = 1
                        html.write('</tr><tr>\n')
                    else :
                        col += 1
                    if col_petites ==  3:
                        col_petites = 1
                        html_petites.write('</tr><tr>\n')
                    else :
                        col_petites += 1
                    ajoute_image(nom,ip,html,html_petites)
                    ok=1 #désolé vince j'ai essayé les exceptions mois c pas si simple
            if not ok:
                #il n'y a pas de logo :-(
                chaines_probleamatiques.append('<b><u>Chaine :</u></b> %s<br><b><u>IP :</u></b> %s:1234<br><br>\n' %(nom,ip))
            print "\t Fait"
            continue
	print 'on s\'occupe de  %s %s' % (ip,nom)
	print '\trecuperation du flux'
        #ip port duree
	os.system('dumpudp %s 1234 2 > /tmp/%s.ts 2>/dev/null &' % (ip,ip))
	print '\tconversion en mpeg1'
	os.system('rm /tmp/%s.mpg 2>/dev/null 1> /dev/null' % (ip)) #eviter le message demandant l'ecrasement
        os.system('ffmpeg -deinterlace -an -i /tmp/%s.ts /tmp/%s.mpg 2>/dev/null 1> /dev/null' % (ip,ip))
        if not os.path.exists('/tmp/%s.mpg' % (ip)) :
            print '\tOn retente'
            os.system('dumpudp %s 1234 2 > /tmp/%s.ts ' % (ip,ip))
            print '\t\tconversion en mpeg1'
            os.system('ffmpeg -deinterlace -an -i /tmp/%s.ts /tmp/%s.mpg 2>/dev/null 1> /dev/null' % (ip,ip))
	print '\tconversion en ppm de la frame 11'
	os.system('rm /tmp/image_%s*ppm 2>/dev/null 2> /dev/null 1> /dev/null' % (ip)) #eviter le message demandant l'ecrasement
	os.system('transcode -q 0 -i /tmp/%s.mpg -x mpeg2,null -y ppm,null -c 10-11 -o /tmp/image_%s 2>/dev/null 1>/dev/null' % (ip,ip))
	print '\tconversion en jpg\n'
	os.system('convert -geometry \'400x300 !\' /tmp/image_%s*ppm /var/www/images/%s.jpg 2>/dev/null 1>/dev/null' % (ip,ip))
	os.system('convert -geometry \'200x150 !\' /tmp/image_%s*ppm /var/www/images/%s_petites.jpg 2>/dev/null 1>/dev/null' % (ip,ip))
	    
	if os.path.exists('/tmp/%s.mpg' % (ip)) : #on teste sur le mpg car il est enleve a chaque fois
            if col == 2 :
                col = 1
                html.write('</tr><tr>\n')
            else :
                col += 1
            if col_petites ==  3:
                col_petites = 1
                html_petites.write('</tr><tr>\n')
            else :
                col_petites += 1
            ajoute_image(nom,ip,html,html_petites)
	else :
	    chaines_probleamatiques.append('<b><u>Chaine :</u></b> %s<br><b><u>IP :</u></b> %s:1234<br><br>\n' %(nom,ip))
            os.system('rm /var/www/images/%s.jpg' % (ip))
            os.system('rm /var/www/images/%s_petites.jpg' % (ip))

	os.system('rm /tmp/%s.ts /tmp/%s.mpg /tmp/image_%s*ppm' % (ip,ip,ip))


    html.write(table_piedpage)
    html_petites.write(table_piedpage)
    #on ecrit les radios
    html.write('<br><u><b><h3>Liste des chaines diffus&eacute;es mais dont le flux est &eacute;rron&eacute; ou uniquement audio (les flux videos ne seront probablement non lisibles avec VLC mais lisibles avec xine)</h3></b></u><br>')
    html_petites.write('<br><u><b><h3>Liste des chaines diffus&eacute;es mais dont le flux est &eacute;rron&eacute; ou uniquement audio (les flux videos ne seront probablement non lisibles avec VLC mais lisibles avec xine)</h3></b></u><br>')
    for line in chaines_probleamatiques :
	html.write(line)
	html_petites.write(line)
    #on ecrit la vraie fin
    html.write(trame_piedpage)
    html.close()
    html_petites.write(trame_piedpage)
    html_petites.close()
    os.system('mv -f /tmp/index.html /var/www/index.html')
    os.system('mv -f /tmp/index_petites.html /var/www/index_petites.html')


if __name__ == '__main__' :
    lock.make_lock('vignettes')
    print 'Vignetisation générale'
    vignettes()
    print 'On a fini'
    lock.remove_lock('vignettes')

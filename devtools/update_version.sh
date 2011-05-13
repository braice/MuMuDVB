#/bin/bash
# $date=date +%Y%m%d
sed -e s/VERSION/`date +%Y%m%d`_`git branch --contains HEAD | grep \* | tr -d "* "`/g configure.ac.template > configure.ac

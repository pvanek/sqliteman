#!/bin/bash
# Create snapshot rpms

BUILDDATE=`date +'%Y%m%d'`

rm ../Sqliteman/CMakeCache.txt
svn2dist ../Sqliteman ../Sqliteman/sqliteman --no-i18n -n sqliteman -v 0.99 --log ./svn2dist.log
cp sqliteman-0.99.tar.gz /usr/src/packages/SOURCES
rpmbuild --define "builddate ${BUILDDATE}" -ba sqliteman.spec

# clean
rm -r /usr/src/packages/BUILD/sqliteman-0.99/

mv /usr/src/packages/RPMS/i586/sqliteman* .
mv /usr/src/packages/SRPMS/sqliteman* .
cp sqliteman-0.99.tar.gz sqliteman-0.99-${BUILDDATE}.tar.gz


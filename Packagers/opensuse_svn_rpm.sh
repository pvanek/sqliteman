#!/bin/bash
# Create snapshot rpms

BUILDDATE=`date +'%Y%m%d'`
VERSION="1.3svn"

rm ../Sqliteman/CMakeCache.txt
./svn2dist ../Sqliteman ../Sqliteman/sqliteman --no-i18n -n sqliteman -v ${VERSION} --log ./svn2dist.log
cp sqliteman-${VERSION}.tar.gz /usr/src/packages/SOURCES
rpmbuild --define "builddate ${BUILDDATE}" -ba sqliteman.spec

# clean
rm -r /usr/src/packages/BUILD/sqliteman-${VERSION}/

mv /usr/src/packages/RPMS/i586/sqliteman* .
mv /usr/src/packages/SRPMS/sqliteman* .
cp sqliteman-${VERSION}.tar.gz sqliteman-${VERSION}-${BUILDDATE}.tar.gz


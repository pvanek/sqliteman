#!/bin/bash
# Create snapshot rpms

VERSION="1.3"

rm ../Sqliteman/CMakeCache.txt
./svn2dist ../Sqliteman ../Sqliteman/sqliteman --no-i18n -n sqliteman -v ${VERSION} --log ./svn2dist.log
cp sqliteman-${VERSION}.tar.* /usr/src/packages/SOURCES
rpmbuild --define "suse_version 1" -ba sqliteman.spec

# clean
rm -r /usr/src/packages/BUILD/sqliteman-${VERSION}/

mv /usr/src/packages/RPMS/*/sqliteman* .
mv /usr/src/packages/SRPMS/sqliteman* .


#!/bin/sh

version=$(echo `cat <../mplayerxp/version.sh | grep version= | cut -d '=' -f2 | cut -d '"' -f2`)
rpm_src=$(echo `rpm --eval '%{_sourcedir}'`)
rpm_home="$rpm_src/../RPMS"
srcdir=$(echo `pwd`)
destdir="$srcdir/upload"

rpm2tgz() {
target_arch=$1
cd $rpm_home/$target_arch
mkdir tmp
cp mplayerxp-$version-1_linux.$target_arch.rpm $destdir
rpm --install --root $rpm_home/$target_arch/tmp --nodeps --ignorearch --ignoreos mplayerxp-$version-1_linux.$target_arch.rpm
cd tmp
tar c usr | gzip -9 >mplayerxp-$version-1_linux.$target_arch.tgz
cp mplayerxp-$version-1_linux.$target_arch.tgz $destdir
cd ..
rm -rf tmp
}

if ! test -f "$rpm_src/mplayerxp-$version-src.tar.bz2" ; then
echo "Error: Source file: $rpm_src/mplayerxp-$version-src.tar.bz2 was not found!"
exit 1
fi

rpmbuild -bb --define "version $version" --target i686-linux mplayerxp.spec
rpmbuild -ba --define "version $version" --target x86_64-linux mplayerxp.spec

install -p -d $destdir

rpm2tgz i686
rpm2tgz x86_64

cd $rpm_home/../SRPMS
cp mplayerxp-$version-1_linux.src.rpm $destdir
cd $rpm_src
cp mplayerxp-$version-src.tar.bz2 $destdir

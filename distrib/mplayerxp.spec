########################################################################################################
# This is a .spec file for building mplayerxp.rpm packages.
#
# Usage:
# rpmbuild -bb --define "version $VERSION" --target i686-linux mplayerxp.spec
# or:
# rpmbuild -ba --define "version $VERSION" --target x86_64-linux mplayerxp.spec
#
# For fast linkage:
# rpmbuild -bb --target CPU-linux --short-circuit mplayerxp.spec
#
# For testing package.rpm:
# rpm -qp --queryformat "%{arch}\n" mplayerxp-*.rpm
# rpm -qp --queryformat "%{os}\n" mplayerxp-*.rpm
########################################################################################################
%define         name    mplayerxp
%define         release 1
%define         prefix  /usr
%define         bindir  %{prefix}/bin
%define         mandir  %{prefix}/man
%define         datadir %{prefix}/share/mplayerxp

Name:           %{name}
Version:        %{version}
Release:        %{release}_%_target_os
Prefix:         %{prefix}
Summary:        Media Player for *nix systems with eXtra Performance
License:        GPL
Group:          Applications/Multimedia
Packager:       Nickols_K <nickols_k@mail.ru>
URL:            http://mplayerxp.sourceforge.net

Source:         %{name}-%{version}-src.tar.bz2

Autoreq:        1

%description
MPlayerXP is a branch of the well known mplayer (http://mplayerhq.hu) which is based on the new thread-based
core. The new core provides better CPU utilization and excellently improves performance of video decoding.
Main goal of this project is to achieve smoothness of video playback due monotonous CPU loading.

%if %_target_cpu==x86_64
%define         bitness 64
%define         lib     lib64
%define         gcc     "gcc -m64"
%define         host    "x86_64-unknown-linux-gnu"
%define         ld_library_path "$LD_LIBRARY_PATH:usr/%{lib}:/usr/%{lib}/xorg"
%define         pkg_config_path "$PKG_CONFIG_PATH:$PKG64_CONFIG_PATH:/usr/local/%{lib}"
%elseif %_target_cpu==i686
%define         bitness 32
%define         lib     lib
%define         gcc     "gcc -m32"
%define         host    "i686-unknown-linux-gnu"
%define         ld_library_path "$LD_LIBRARY_PATH:usr/%{lib}:/usr/%{lib}/xorg"
%define         pkg_config_path "$PKG_CONFIG_PATH:$PKG32_CONFIG_PATH:/usr/local/%{lib}"
%else
# generic or unknown arch-os
%define         gcc     "gcc"
%endif

%prep
%setup -q

%build
export LD_LIBRARY_PATH=%{ld_library_path}
export PKG_CONFIG_PATH=%{pkg_config_path}
DESTDIR=$RPM_BUILD_ROOT CFLAGS=-O3 CC=%{gcc} ./configure --prefix=%{prefix} --host=%{host} --program-suffix=%{bitness}-%{version}
make

%install
rm -rf $RPM_BUILD_ROOT
make install
# addon from author of mplayerxp
install -p -d $RPM_BUILD_ROOT%{datadir}/font
cp -r /usr/local/share/mplayerxp/font $RPM_BUILD_ROOT%{datadir}
cd $RPM_BUILD_ROOT%{bindir}
ln -s mplayerxp%{bitness}-%{version} mplayerxp%{bitness}
ln -s mplayerxp%{bitness} mplayerxp

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%attr(0755, root, root) %{bindir}/mplayerxp%{bitness}-%{version}
%{bindir}/mplayerxp%{bitness}
%{bindir}/mplayerxp
%{mandir}/man?/*
%{datadir}/codecs.conf
%{datadir}/menu.conf
%{datadir}/eqbands
%{datadir}/font/*

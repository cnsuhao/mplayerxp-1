########################################################################################################
# This is a .spec file for building mplayerxp.rpm packages.
# Usage: rpm -bb mplayerxp.spec
########################################################################################################
%define         x86_64  1
%define         name    mplayerxp
%define         version 0.7.95
%define         release 1
%define         prefix  /usr
%define         bindir  %{prefix}/bin
%define         mandir  %{prefix}/man
%define         datadir %{prefix}/share/mplayerxp

Name:           %{name}
Version:        %{version}
Release:        %{release}
Prefix:         %{prefix}
Summary:        Media Player for *nix systems with eXtra Performance.
License:        GPL
Group:          Applications/Multimedia
Packager:       Nickols_K <nickols_k@mail.ru>
URL:            http://mplayerxp.sourceforge.net

Source:         %{name}-%{version}-src.tar.bz2

%description
MPlayerXP is a branch of the well known mplayer (http://mplayerhq.hu) which is based on the new thread-based
core. The new core provides better CPU utilization and excellently improves performance of video decoding.
Main goal of this project is to achieve smoothness of video playback due monotonous CPU loading.

%if %{x86_64}
%define         bitness 64
%define         lib     lib64
%define         gcc     "gcc -m64"
%define         host    x86_64-unknown-linux-gnu
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:usr/%{lib}:/usr/%{lib}/xorg"
export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$PKG64_CONFIG_PATH:/usr/local/%{lib}"
%else
%define         bitness 32
%define         lib     lib
%define         gcc     "gcc -m32"
%define         host    i686-unknown-linux-gnu
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:usr/%{lib}:/usr/%{lib}/xorg"
export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$PKG32_CONFIG_PATH:/usr/local/%{lib}"
%endif

%prep
%setup -q

%build
DESTDIR=$RPM_BUILD_ROOT CFLAGS=-O3 CC=%{gcc} ./configure --prefix=%{prefix} --host=%{host} --program-suffix=%{bitness}-%{version}
make

%install
rm -rf $RPM_BUILD_ROOT
make install
# addon from author of mplayerxp
install -p -d $RPM_BUILD_ROOT%{datadir}/font
cp -r /usr/local/share/mplayerxp/font $RPM_BUILD_ROOT%{datadir}/font
cd $RPM_BUILD_ROOT%{bindir}
ln -s mplayerxp{bitness}-%{version} mplayerxp{bitness}
ln -s mplayerxp{bitness} mplayerxp

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%attr(-, root, root, 0755) %{bindir}/mplayerxp{bitness}-%{version}
%{bindir}/mplayerxp{bitness}
%{bindir}/mplayerxp
%{mandir}/man?/*
%{datadir}/*
%{datadir}/fonts/*

%define fedora 7
%define rhel 0

Summary: 	X Neural Switcher
Name:		xneur
Version: 	0.9.5
Release: 	0%{?dist}

License: 	GPL
Group: 		Applications/Office
URL: 		http://www.xneur.ru
Source: 	%{name}-%{version}.tar.bz2

BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

Requires:	aspell
BuildRequires: 	glib2-devel aspell-devel
BuildRequires: 	pkgconfig >= 0.20
Requires:	pcre >= 5.0
BuildRequires: 	pcre-devel >= 5.0
%if 0%{?fedora} > 4 || 0%{?rhel} > 4
Requires:	libX11
Requires:	libXpm
BuildRequires: 	libX11-devel
BuildRequires: 	libXpm-devel
%else
Requires:	xorg-x11
BuildRequires: 	xorg-x11-devel
%endif
%if 0%{?fedora} > 0
Requires:	freealut > 1.0.1
BuildRequires: 	freealut-devel
Requires:	gstreamer >= 0.10.6
BuildRequires: 	gstreamer-devel
%endif


%description
It's program like Punto Switcher, but has other functionally and features
for configurate.


%package devel
Summary: 	Static library and header files for the sgutils library
Group:          Applications/Office
Requires: 	%{name} = %{version}


%description devel
This package contains the static %{name} library and its header files for
developing applications.


%prep
rm -rf $RPM_BUILD_ROOT
%setup -q

%build
%if 0%{?fedora} > 6
./autogen.sh
%else
./autogen.sh --without-sound
%endif
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
%makeinstall
%if %{_arch} == x86_64
mv -f $RPM_BUILD_ROOT/usr/lib/pkgconfig $RPM_BUILD_ROOT/usr/lib64/
%endif

mkdir -p ${RPM_BUILD_ROOT}/usr/local/share/
ln -s /usr/share/xneur ${RPM_BUILD_ROOT}/usr/local/share/xneur


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root)
%doc AUTHORS ChangeLog NEWS README
%{_datadir}/%{name}/*
%{_bindir}/*
%{_libdir}/*.so.*
%{_mandir}/man1/*
/usr/local/share/*


%files devel
%defattr(-,root,root)
%{_libdir}/*.so
%{_libdir}/*.la
%{_libdir}/*.a
%{_libdir}/pkgconfig/*.pc
%{_includedir}/%{name}/*.h

%changelog
* Mon Jun 30 2008 Yankin Nikolay <web@softodrom.ru> 0.9.1
- update to 0.9.1

* Tue Jun 24 2008 Vadim Likhota <vadiml.xx@gmail.com> 0.9.0
- update to 0.9.0
- test on fc7 && asp12

* Thu Oct 11 2007 Vadim Likhota <vadim-lvv@yandex.ru> 0.8
- update to 0.8.0

* Tue Jul 17 2007 Vadim Likhota <vadim-lvv@yandex.ru> 0.6.2-1
- update & test for centos5 x86_64

* Wed Jul 04 2007 Vadim Likhota <vadim-lvv@yandex.ru> 0.6.2-0
- to 0.6.2

* Sat Jun 02 2007 Vadim Likhota <vadim-lvv@yandex.ru> 0.6.1-1
- add xneur.man

* Thu May 17 2007 Vadim Likhota <vadim-lvv@yandex.ru> 0.6.1-0
- update to 0.6.1 from svn

* Mon Apr 23 2007 Arkady L. Shane <ashejn@yandex-team.ru> 0.6-1
- 0.6.0

* Sat Mar 10 2007 Arkady L. Shane <ashejn@yandex-team.ru> 0.5-1
- 0.5.0
- add devel package

* Tue Jan 23 2007 Arkady L. Shane <ashejn@yandex-team.ru> 0.4-1
- rebuilt for FC6
- cleanup spec

* Wed Jan 03 2007 Nik <niktr@mail.ru>
- rebuild for FC6
- updated to svn version dated 03012007
- minor changes in spec file

* Wed Mar 02 2005 Andy Shevchenko <andriy@asplinux.ru>
- rebuild for ASPLinux
- update to 0.0.3

* Mon Feb 14 2005 myLinux, Ltd <info@mylinux.com.ua>
- build for myLinux

* Wed Jan 12 2005 Vitaly Lipatov <lav@altlinux.ru> 0.0.2-alt0.1
- first build for Sisyphus

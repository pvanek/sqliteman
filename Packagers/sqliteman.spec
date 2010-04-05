#
# spec file for package Sqliteman
#
# Copyright (c) 2007 Petr Vanek
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#

Name:         sqliteman
URL:          http://sqliteman.com
License:      GPL
Group:        Productivity/Databases/Tools
Summary:      Lightweigth but powerful Sqlite3 manager. Development snapshot
Version:      1.3
Release:      20100404
Source0:       %{name}-%{version}.tar.bz2
#Source1:      %{name}.desktop

%if 0%{?suse_version}
Requires:     libqt4 >= 4.2.0 libqt4-sql-sqlite >= 4.2.0 libicu libuuid1
BuildRequires: libqt4-devel >= 4.2.0 libqt4-sql-sqlite cmake libicu-devel libuuid-devel update-desktop-files desktop-file-utils
%endif

#%if 0%{?fedora_version}
%if %{_target_vendor} == redhat
Requires:     qt4 >= 4.2.0 qt4-sqlite >= 4.2.0
BuildRequires: qt4-devel >= 4.2.0 qt4-sqlite cmake >- 2.6.0 gcc-c++ update-desktop-files desktop-file-utils
%endif


BuildRoot:    %{_tmppath}/%{name}-%{version}-build

%description
Warning: this is a bleeding edge development version. You
can use stable 1.2 version if you observe any problems.
The best developer's and/or admin's GUI tool for Sqlite3
in the world. No joking here (or just a bit only) - it
contains the most complette feature set of all tools available


Authors:
--------
    Petr Vanek <petr@scribus.info>

%prep
%setup -q

%build
%if 0%{?suse_version}
cmake \
        -DCMAKE_C_FLAGS="%{optflags}" \
        -DCMAKE_CXX_FLAGS="%{optflags}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=%{_prefix} \
        -DWANT_INTERNAL_QSCINTILLA=1 \
        %{_builddir}/%{name}-%{version}
%endif

#%if 0%{?fedora_version}
%if %{_target_vendor} == redhat
cmake \
        -DCMAKE_C_FLAGS="%{optflags}" \
        -DCMAKE_CXX_FLAGS="%{optflags}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=%{buildroot}/usr \
        -DWANT_INTERNAL_QSCINTILLA=1 \
        %{_builddir}/%{name}-%{version}
%endif

%{__make} %{?jobs:-j%jobs}


%install
%makeinstall


%if 0%{?suse_version}  
#desktop-file-install  %{SOURCE1} %{buildroot}/usr/share/applications/sqliteman.desktop \
%suse_update_desktop_file %{buildroot}/usr/share/applications/sqliteman.desktop  Office Database \
%endif

%if 0%{?fedora_version} >= 5  
 %{__install} -d -m0755 %{buildroot}%{_datadir}/applications/  
     desktop-file-install --vendor %{desktop_vendor}    \  
         --add-category X-Red-Hat-Base              \  
         --dir %{buildroot}%{_datadir}/applications \  
          %name.desktop
%endif

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root)
%{_bindir}/*
%{_prefix}/share/applications/*
#%{_prefix}/share/icons/*
%{_prefix}/share/sqliteman/
%{_prefix}/share/sqliteman/*
%{_libdir}/sqliteman/
%{_libdir}/sqliteman/*
/usr/share/man/man?/*.*  


%changelog -n sqliteman
* Sat Apr 04 2010 - Petr Vanek <petr@scribus.info>
- fixed rpm group to fit opensuse categories

* Tue Jun 12 2007 - Petr Vanek <petr@scribus.info>
- merging FC and Suse specs into one. Opensuse Build service works

* Fri Apr 11 2007 - Eugene Pivnev <ti.eugene@gmail.com>
- initial build for FC6

* Wed Feb 20 2007 - Petr Vanek <petr@scribus.info>
- initial package

  

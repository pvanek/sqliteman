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
Group:        Development/Databases
Summary:      Lightweigth but powerfull Sqlite3 manager. Development snapshot.
Version:      1.3.svn
Release:      %{builddate}
Source:       %{name}-%{version}.tar.gz

%if 0%{?suse_version}
Requires:     libqt4 >= 4.3.0 libqt4-sql-sqlite >= 4.3.0 sqlite
BuildRequires: libqt4-devel >= 4.3.0 libqt4-sql-sqlite cmake >= 2.6.0
%endif

#%if 0%{?fedora_version}
%if %{_target_vendor} == redhat
Requires:     qt4 >= 4.3.0 qt4-sqlite >= 4.3.0
BuildRequires: qt4-devel >= 4.3.0 qt4-sqlite cmake 2.6.0 gcc-c++
%endif


BuildRoot:    %{_tmppath}/%{name}-%{version}-build

%description
Warning: this is a bleeding edge development version. You
can use stable 1.2 version if you observe any problems.
The best developer's and/or admin's GUI tool for Sqlite3
in the world. No joking here (or just a bit only) - it
contains the most complette feature set of all tools available.


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

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root)
%{_bindir}
%{_datadir}


%changelog -n sqliteman
* Tue Jun 12 2007 - Petr Vanek <petr@scribus.info>
- merging FC and Suse specs into one. Opensuse Build service works

* Fri Apr 11 2007 - Eugene Pivnev <ti.eugene@gmail.com>
- initial build for FC6

* Wed Feb 20 2007 - Petr Vanek <petr@scribus.info>
- initial package

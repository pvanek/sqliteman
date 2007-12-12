#
# spec file for package Sqliteman
#
# Copyright (c) 2007 Petr Vanek
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#

Name:         sqliteman
URL:          http://www.assembla.com/space/sqliteman
License:      GPL
Group:        Development/Databases
Summary:      Lightweigth but powerfull Sqlite3 manager. Development snapshot.
Version:      1.1
Release:      %{builddate}
Source:       %{name}-%{version}.tar.gz

%if 0%{?suse_version}
Requires:     libqt4 >= 4.3.0 libqt4-sql-sqlite >= 4.3.0 sqlite
BuildRequires: libqt4-devel >= 4.3.0 libqt4-sql-sqlite cmake
%endif

#%if 0%{?fedora_version}
%if %{_target_vendor} == redhat
Requires:     qt4 >= 4.3.0 qt4-sqlite >= 4.3.0
BuildRequires: qt4-devel >= 4.3.0 qt4-sqlite cmake gcc-c++
%endif


BuildRoot:    %{_tmppath}/%{name}-%{version}-build

%description
Warning: this is a bleeding edge development version. You
can use stable 1.0 version if you observe any problems.
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
%if 0%{?suse_version}
%{_bindir}/sqliteman
%{_datadir}/applications/
%{_datadir}/applications/sqliteman.desktop
%{_datadir}/icons/
%{_datadir}/icons/sqliteman.png
%{_datadir}/sqliteman/
%{_datadir}/sqliteman/icons/
%{_datadir}/sqliteman/icons/clear_table_contents.png
%{_datadir}/sqliteman/icons/column.png
%{_datadir}/sqliteman/icons/document-new.png
%{_datadir}/sqliteman/icons/document-save.png
%{_datadir}/sqliteman/icons/key.png
%{_datadir}/sqliteman/icons/sqliteman.png
%{_datadir}/sqliteman/icons/trigger.png
%{_datadir}/sqliteman/icons/database.png
%{_datadir}/sqliteman/icons/document-open.png
%{_datadir}/sqliteman/icons/index.png
%{_datadir}/sqliteman/icons/runexplain.png
%{_datadir}/sqliteman/icons/system.png
%{_datadir}/sqliteman/icons/view.png
%{_datadir}/sqliteman/icons/delete_table_row.png
%{_datadir}/sqliteman/icons/document-save-as.png
%{_datadir}/sqliteman/icons/insert_table_row.png
%{_datadir}/sqliteman/icons/runsql.png
%{_datadir}/sqliteman/icons/table.png
%{_datadir}/sqliteman/sqliteman_cs.qm
%{_datadir}/sqliteman/sqliteman_de.qm
%{_datadir}/sqliteman/sqliteman_pl.qm
%{_datadir}/sqliteman/doc/en/index.html
%{_datadir}/sqliteman/doc/en/license.html
%{_datadir}/sqliteman/doc/en/menu.html
%{_datadir}/sqliteman/doc/en/results.html
%{_datadir}/sqliteman/doc/en/sqleditor.html
%{_datadir}/sqliteman/doc/en/sqlite.html
%{_datadir}/sqliteman/doc/en/troubleshooting.html
%{_datadir}/sqliteman/doc/en/usage-context.html
%{_datadir}/sqliteman/doc/en/usage-database.html
%{_datadir}/sqliteman/doc/en/usage-file.html
%{_datadir}/sqliteman/doc/en/usage-prefs.html
%{_datadir}/sqliteman/doc/en/usage-run.html
%{_datadir}/sqliteman/doc/en/usage-system.html
%{_datadir}/sqliteman/doc/en/main_window.png
%{_datadir}/sqliteman/doc/en/results.png
%{_datadir}/sqliteman/doc/en/sqleditor.png
%endif

%changelog -n sqliteman
* Tue Jun 12 2007 - Petr Vanek <petr@scribus.info>
- merging FC and Suse specs into one. Opensuse Build service works

* Fri Apr 11 2007 - Eugene Pivnev <ti.eugene@gmail.com>
- initial build for FC6

* Wed Feb 20 2007 - Petr Vanek <petr@scribus.info>
- initial package

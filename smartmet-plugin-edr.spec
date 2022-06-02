%bcond_without observation
%define DIRNAME edr
%define SPECNAME smartmet-plugin-%{DIRNAME}
Summary: SmartMet edr plugin
Name: %{SPECNAME}
Version: 22.6.2
Release: 1%{?dist}.fmi
License: MIT
Group: SmartMet/Plugins
URL: https://github.com/fmidev/smartmet-plugin-edr
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: rpm-build
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: boost169-devel
BuildRequires: fmt-devel >= 7.1.3
BuildRequires: bzip2-devel
BuildRequires: zlib-devel
BuildRequires: smartmet-library-edr-devel >= 22.5.24
BuildRequires: smartmet-library-spine-devel >= 22.5.24
BuildRequires: smartmet-library-locus-devel >= 22.3.28
BuildRequires: smartmet-library-macgyver-devel >= 22.5.24
BuildRequires: smartmet-library-grid-content-devel >= 22.5.24
BuildRequires: smartmet-library-grid-files-devel >= 22.5.24
BuildRequires: smartmet-library-newbase-devel >= 22.5.24
BuildRequires: smartmet-library-gis-devel >= 22.5.4
BuildRequires: smartmet-engine-geonames-devel >= 22.5.24
%if %{with observation}
BuildRequires: smartmet-engine-observation-devel >= 22.6.2
%endif
BuildRequires: smartmet-engine-querydata-devel >= 22.5.31
BuildRequires: smartmet-engine-gis-devel >= 22.5.24
BuildRequires: smartmet-engine-grid-devel >= 22.5.24
# obsengine can be disabled in configuration: not included intentionally
#%if %{with observation}
#Requires: smartmet-engine-observation >= 22.6.2
#%endif
Requires: fmt >= 7.1.3
Requires: smartmet-library-gis >= 22.5.4
Requires: smartmet-library-locus >= 22.3.28
Requires: smartmet-library-macgyver >= 22.5.24
Requires: smartmet-library-newbase >= 22.5.24
Requires: smartmet-library-spine >= 22.5.24
Requires: smartmet-library-edr >= 22.5.24
Requires: smartmet-library-gis >= 22.5.4
Requires: smartmet-engine-geonames >= 22.5.24
Requires: smartmet-engine-querydata >= 22.5.31
Requires: smartmet-engine-gis >= 22.5.24
Requires: smartmet-engine-grid >= 22.5.24
Requires: smartmet-server >= 22.5.16
Requires: boost169-date-time
Requires: boost169-filesystem
Requires: boost169-iostreams
Requires: boost169-system
Requires: boost169-thread
Provides: %{SPECNAME}
#TestRequires: smartmet-utils-devel >= 22.1.20
#TestRequires: smartmet-library-spine-plugin-test >= 22.4.26
#TestRequires: smartmet-library-newbase-devel >= 22.5.24
#TestRequires: redis
#TestRequires: smartmet-test-db >= 21.1.21
#TestRequires: smartmet-test-data >= 20.6.30
#TestRequires: smartmet-engine-grid-test >= 21.1.21
#TestRequires: smartmet-library-gis >= 22.5.4
#TestRequires: smartmet-engine-geonames >= 22.5.24
#TestRequires: smartmet-engine-gis >= 22.5.24
#TestRequires: smartmet-engine-querydata >= 22.5.31
%if %{with observation}
#TestRequires: smartmet-engine-observation >= 22.6.2
%endif
#TestRequires: smartmet-engine-grid >= 22.5.24
#TestRequires: gdal34

%description
SmartMet edr plugin

%prep
rm -rf $RPM_BUILD_ROOT

%setup -q -n %{SPECNAME}
 
%build -q -n %{SPECNAME}
make %{_smp_mflags} \
     %{?!with_observation:CFLAGS=-DWITHOUT_OBSERVATION}

%install
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(0775,root,root,0775)
%{_datadir}/smartmet/plugins/edr.so

%changelog
* Thu Jun 2 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.6.2-1.fmi
- First release (BRAINSTORM-2308)

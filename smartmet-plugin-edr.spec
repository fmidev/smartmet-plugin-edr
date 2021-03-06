%bcond_without observation
%define DIRNAME edr
%define SPECNAME smartmet-plugin-%{DIRNAME}
Summary: SmartMet edr plugin
Name: %{SPECNAME}
Version: 22.6.21
Release: 1%{?dist}.fmi
License: MIT
Group: SmartMet/Plugins
URL: https://github.com/fmidev/smartmet-plugin-edr
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
%if 0%{?rhel} && 0%{rhel} < 9
%define smartmet_boost boost169
%else
%define smartmet_boost boost
%endif

%define smartmet_fmt_min 8.1.1
%define smartmet_fmt_max 8.2.0

BuildRequires: rpm-build
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: %{smartmet_boost}-devel
BuildRequires: fmt-devel >= %{smartmet_fmt_min}, fmt-devel < %{smartmet_fmt_max}
BuildRequires: bzip2-devel
BuildRequires: zlib-devel
BuildRequires: smartmet-library-spine-devel >= 22.6.16
BuildRequires: smartmet-library-locus-devel >= 22.6.16
BuildRequires: smartmet-library-macgyver-devel >= 22.6.16
BuildRequires: smartmet-library-grid-content-devel >= 22.5.24
BuildRequires: smartmet-library-grid-files-devel >= 22.5.24
BuildRequires: smartmet-library-newbase-devel >= 22.6.16
BuildRequires: smartmet-library-gis-devel >= 22.6.16
BuildRequires: smartmet-engine-geonames-devel >= 22.6.17
%if %{with observation}
BuildRequires: smartmet-engine-observation-devel >= 22.6.17
%endif
BuildRequires: smartmet-engine-querydata-devel >= 22.6.17
BuildRequires: smartmet-engine-gis-devel >= 22.6.17
BuildRequires: smartmet-engine-grid-devel >= 22.6.17
# obsengine can be disabled in configuration: not included intentionally
#%if %{with observation}
#Requires: smartmet-engine-observation >= 22.6.17
#%endif
Requires: fmt >= %{smartmet_fmt_min}, fmt < %{smartmet_fmt_max}
Requires: smartmet-library-gis >= 22.6.16
Requires: smartmet-library-locus >= 22.6.16
Requires: smartmet-library-macgyver >= 22.6.16
Requires: smartmet-library-newbase >= 22.6.16
Requires: smartmet-library-spine >= 22.6.16
Requires: smartmet-library-gis >= 22.6.16
Requires: smartmet-engine-geonames >= 22.6.17
Requires: smartmet-engine-querydata >= 22.6.17
Requires: smartmet-engine-gis >= 22.6.17
Requires: smartmet-engine-grid >= 22.6.17
Requires: smartmet-server >= 22.5.16
Requires: %{smartmet_boost}-date-time
Requires: %{smartmet_boost}-filesystem
Requires: %{smartmet_boost}-iostreams
Requires: %{smartmet_boost}-system
Requires: %{smartmet_boost}-thread
Provides: %{SPECNAME}
#TestRequires: smartmet-utils-devel >= 22.1.20
#TestRequires: smartmet-library-spine-plugin-test >= 22.6.16
#TestRequires: smartmet-library-newbase-devel >= 22.6.16
#TestRequires: redis
#TestRequires: smartmet-test-db >= 21.1.21
#TestRequires: smartmet-test-data >= 20.6.30
#TestRequires: smartmet-engine-grid-test >= 22.6.17
#TestRequires: smartmet-library-gis >= 22.6.16
#TestRequires: smartmet-engine-geonames >= 22.6.17
#TestRequires: smartmet-engine-gis >= 22.6.17
#TestRequires: smartmet-engine-querydata >= 22.6.17
%if %{with observation}
#TestRequires: smartmet-engine-observation >= 22.6.17
%endif
#TestRequires: smartmet-engine-grid >= 22.6.17
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
* Tue Jun 21 2022 Andris Pav??nis <andris.pavenis@fmi.fi> 22.6.21-1.fmi
- Add support for RHEL9, upgrade libpqxx to 7.7.0 (rhel8+) and fmt to 8.1.1

* Mon Jun 20 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.6.20-1.fmi
- ABI fixes

* Wed Jun 15 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.6.15-1.fmi
- Testcases added, checking unknown parameters before query.

* Fri Jun 10 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.6.10-1.fmi
- Changed output format of multipoint/polygon query from MultiPointSeries to CoverageCollection
- Implemented 'collections/{collection_id}/instances' metadata query

* Thu Jun 2 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.6.2-1.fmi
- First release (BRAINSTORM-2308)

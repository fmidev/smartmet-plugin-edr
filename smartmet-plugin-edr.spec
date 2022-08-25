%bcond_without observation
%define DIRNAME edr
%define SPECNAME smartmet-plugin-%{DIRNAME}
Summary: SmartMet edr plugin
Name: %{SPECNAME}
Version: 22.8.25
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
BuildRequires: smartmet-library-spine-devel >= 22.8.25
BuildRequires: smartmet-library-locus-devel >= 22.6.17
BuildRequires: smartmet-library-macgyver-devel >= 22.8.23
BuildRequires: smartmet-library-grid-content-devel >= 22.8.24
BuildRequires: smartmet-library-grid-files-devel >= 22.6.23
BuildRequires: smartmet-library-newbase-devel >= 22.8.24
BuildRequires: smartmet-library-gis-devel >= 22.7.27
BuildRequires: smartmet-engine-geonames-devel >= 22.8.4
%if %{with observation}
BuildRequires: smartmet-engine-observation-devel >= 22.7.28
%endif
BuildRequires: smartmet-engine-querydata-devel >= 22.8.24
BuildRequires: smartmet-engine-gis-devel >= 22.8.1
BuildRequires: smartmet-engine-grid-devel >= 22.8.24
# obsengine can be disabled in configuration: not included intentionally
#%if %{with observation}
#Requires: smartmet-engine-observation >= 22.7.28
#%endif
Requires: fmt >= %{smartmet_fmt_min}, fmt < %{smartmet_fmt_max}
Requires: smartmet-library-gis >= 22.7.27
Requires: smartmet-library-locus >= 22.6.17
Requires: smartmet-library-macgyver >= 22.8.23
Requires: smartmet-library-newbase >= 22.8.24
Requires: smartmet-library-spine >= 22.8.25
Requires: smartmet-library-gis >= 22.7.27
Requires: smartmet-engine-geonames >= 22.8.4
Requires: smartmet-engine-querydata >= 22.8.24
Requires: smartmet-engine-gis >= 22.8.1
Requires: smartmet-engine-grid >= 22.8.24
Requires: smartmet-server >= 22.8.19
Requires: %{smartmet_boost}-date-time
Requires: %{smartmet_boost}-filesystem
Requires: %{smartmet_boost}-iostreams
Requires: %{smartmet_boost}-system
Requires: %{smartmet_boost}-thread
Provides: %{SPECNAME}
#TestRequires: smartmet-utils-devel >= 22.2.8
#TestRequires: smartmet-library-spine-plugin-test >= 22.8.25
#TestRequires: smartmet-library-newbase-devel >= 22.8.24
#TestRequires: redis
#TestRequires: smartmet-test-db >= 22.4.14
#TestRequires: smartmet-test-data >= 20.12.1
#TestRequires: smartmet-engine-grid-test >= 22.8.24
#TestRequires: smartmet-library-gis >= 22.7.27
#TestRequires: smartmet-engine-geonames >= 22.8.4
#TestRequires: smartmet-engine-gis >= 22.8.1
#TestRequires: smartmet-engine-querydata >= 22.8.24
%if %{with observation}
#TestRequires: smartmet-engine-observation >= 22.7.28
%endif
#TestRequires: smartmet-engine-grid >= 22.8.24
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
* Thu Aug 25 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.25-1.fmi
- Use a generic exception handler for configuration file errors

* Thu Aug 18 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.8.18-1.fmi
- Removed redundant coords-perameter from data_queries metadata
- Fixed metadata temporal interval format
- Changed date format to iso extended format
- Fixed within-unit to within-units in Radius query

* Tue Aug 9 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.8.9-1.fmi
- Fixed hardcoded hostname
- Added data_queries section to metadata
- Implemented Locations-matadata query
- Fixed Content-Type to application/json

* Thu Jul 28 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.7.28-1.fmi
- Repackaged due to QEngine ABI change

* Wed Jul 27 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.7.27-1.fmi
- Repackaged since macgyver CacheStats ABI changed

* Tue Jun 21 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.6.21-1.fmi
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

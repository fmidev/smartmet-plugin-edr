%bcond_without observation
%define DIRNAME edr
%define SPECNAME smartmet-plugin-%{DIRNAME}
Summary: SmartMet edr plugin
Name: %{SPECNAME}
Version: 23.1.26
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
BuildRequires: jsoncpp-devel >= 1.8.4
BuildRequires: smartmet-library-spine-devel >= 22.12.2
BuildRequires: smartmet-library-locus-devel >= 22.12.16
BuildRequires: smartmet-library-macgyver-devel >= 22.12.16
BuildRequires: smartmet-library-grid-content-devel >= 22.12.12
BuildRequires: smartmet-library-grid-files-devel >= 22.12.19
BuildRequires: smartmet-library-newbase-devel >= 22.11.14
BuildRequires: smartmet-library-gis-devel >= 22.12.21
BuildRequires: smartmet-library-timeseries-devel >= 23.1.26
BuildRequires: smartmet-engine-geonames-devel >= 22.12.21
%if %{with observation}
BuildRequires: smartmet-engine-observation-devel >= 23.1.26
%endif
BuildRequires: smartmet-engine-querydata-devel >= 22.12.15
BuildRequires: smartmet-engine-gis-devel >= 22.12.21
BuildRequires: smartmet-engine-grid-devel >= 22.12.12
# obsengine can be disabled in configuration: not included intentionally
#%if %{with observation}
#Requires: smartmet-engine-observation >= 23.1.26
#%endif
Requires: fmt >= %{smartmet_fmt_min}, fmt < %{smartmet_fmt_max}
Requires: jsoncpp
Requires: smartmet-library-gis >= 22.12.21
Requires: smartmet-library-locus >= 22.12.16
Requires: smartmet-library-macgyver >= 22.12.16
Requires: smartmet-library-newbase >= 22.11.14
Requires: smartmet-library-spine >= 22.12.2
Requires: smartmet-library-timeseries >= 23.1.26
Requires: smartmet-library-gis >= 22.12.21
Requires: smartmet-engine-geonames >= 22.12.21
Requires: smartmet-engine-querydata >= 22.12.15
Requires: smartmet-engine-gis >= 22.12.21
Requires: smartmet-engine-grid >= 22.12.12
Requires: smartmet-server >= 22.12.5
Requires: %{smartmet_boost}-date-time
Requires: %{smartmet_boost}-filesystem
Requires: %{smartmet_boost}-iostreams
Requires: %{smartmet_boost}-system
Requires: %{smartmet_boost}-thread
Provides: %{SPECNAME}
#TestRequires: smartmet-utils-devel >= 22.12.14
#TestRequires: smartmet-library-spine-plugin-test >= 22.12.2
#TestRequires: smartmet-library-newbase-devel >= 22.11.14
#TestRequires: redis
#TestRequires: smartmet-test-db >= 22.4.14
#TestRequires: smartmet-test-data >= 20.12.1
#TestRequires: smartmet-engine-grid-test >= 22.12.12
#TestRequires: smartmet-library-gis >= 22.12.21
#TestRequires: smartmet-engine-geonames >= 22.12.21
#TestRequires: smartmet-engine-gis >= 22.12.21
#TestRequires: smartmet-engine-querydata >= 22.12.15
%if %{with observation}
#TestRequires: smartmet-engine-observation >= 23.1.26
%endif
#TestRequires: smartmet-engine-grid >= 22.12.12
#TestRequires: gdal34
#TestRequires: libwebp13

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
* Thu Jan 26 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.1.26-1.fmi
- Added support for request size limits (BRAINSTORM-2443)

* Thu Dec 22 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.12.22-1.fmi
- Fixed several issues reported by CodeChecker

* Tue Dec 13 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.12.13-1.fmi
- Fixed bug of missing observation collections in  <host>/edr/collections metadata query

* Mon Dec 12 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.12.12-2.fmi
- Timestamp of the latest metadata update added into hash_value of metadata query

* Mon Dec 12 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.12.12-1.fmi
- Repackaged due to ABI changes

* Mon Dec  5 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.12.5-1.fmi
- Check HTTP request type and handle only POST and OPTIONS requests

* Mon Nov 28 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.11.28-1.fmi
- Additional parameter info can be added in the configuration file

* Fri Nov 18 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.11.18-1.fmi
- Temporal interval format in metadata fixed
- Locations query fixed
- Document updated

* Mon Nov 14 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.11.14-1.fmi
- Fixed Position query for collection instnces
- Valid locations in Location query can now be defined in configuration file by using keywords
- More information of locations added into metadata (properties section)

* Tue Nov 8 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.11.8-1.fmi
- Code cleaned, started writing plugin documentation

* Thu Oct 27 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.10.27-1.fmi
- Added support for 2D Corridor query
- Added metadata update thread: two new configuration parameters: metadata_updates_disabled, metadata_update_interval
- Coded own JSON-parser, compliant with jsoncpp Value-class, except that precision can be defined for double fields

* Thu Oct 20 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.20-1.fmi
- Repackaged due to base library ABI changes

* Mon Oct 17 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.10.10-1.fmi
- Added support for Cube query

* Mon Oct 10 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.10.10-1.fmi
- Fixed segmentation fault bug when observation engine is disabled

* Wed Oct 5 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.10.5-1.fmi
- Reintroduce description-field in parameter section and set parameter name into it

* Wed Sep 21 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.9.21-1.fmi
- Removed slow observablePropertyQuery database query for each parameter, 
since description field is not used any more

* Tue Sep 20 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.9.20-2.fmi
- Timestamp is added into hash_value of metadata queries, timestamp is updated every 300 seconds

* Tue Sep 20 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.9.20-1.fmi
- Fixed hash_value calculation (request.getResource added)

* Mon Sep 19 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.9.19-1.fmi
- Remove description-field from parameter section and set parameter name into label-field

* Fri Sep 16 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.9.16-1.fmi
- When z-parameter not specified all levels are returned
- Modified output format of trajectory query
- Added support for 3D and 4D trajectory queries

* Fri Sep  9 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.9.9-1.fmi
- Repackaged since TimeSeries library ABI changed

* Thu Sep  8 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.9.8-1.fmi
- Fixed a string concatenation error

* Thu Sep 1 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.9.1-1.fmi
- Added support for grid-engine collections
- Added support for 2D trajectory data queries

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

%bcond_without observation
%define DIRNAME edr
%define SPECNAME smartmet-plugin-%{DIRNAME}
Summary: SmartMet edr plugin
Name: %{SPECNAME}
Version: 23.5.24
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
BuildRequires: smartmet-library-spine-devel >= 23.4.27
BuildRequires: smartmet-library-locus-devel >= 23.3.7
BuildRequires: smartmet-library-macgyver-devel >= 23.5.24
BuildRequires: smartmet-library-grid-content-devel >= 23.5.26
BuildRequires: smartmet-library-grid-files-devel >= 23.3.9
BuildRequires: smartmet-library-newbase-devel >= 23.2.9
BuildRequires: smartmet-library-gis-devel >= 23.3.14
BuildRequires: smartmet-library-timeseries-devel >= 23.3.15
BuildRequires: smartmet-engine-geonames-devel >= 23.4.27
%if %{with observation}
BuildRequires: smartmet-engine-observation-devel >= 23.6.1
%endif
BuildRequires: smartmet-engine-querydata-devel >= 23.4.27
BuildRequires: smartmet-engine-gis-devel >= 22.12.21
BuildRequires: smartmet-engine-grid-devel >= 23.5.26
# obsengine can be disabled in configuration: not included intentionally
#%if %{with observation}
#Requires: smartmet-engine-observation >= 23.6.1
#%endif
Requires: fmt >= %{smartmet_fmt_min}, fmt < %{smartmet_fmt_max}
Requires: jsoncpp
Requires: smartmet-library-gis >= 23.3.14
Requires: smartmet-library-locus >= 23.3.7
Requires: smartmet-library-macgyver >= 23.5.24
Requires: smartmet-library-newbase >= 23.2.9
Requires: smartmet-library-spine >= 23.4.27
Requires: smartmet-library-timeseries >= 23.3.15
Requires: smartmet-library-gis >= 23.3.14
Requires: smartmet-engine-geonames >= 23.4.27
Requires: smartmet-engine-querydata >= 23.4.27
Requires: smartmet-engine-gis >= 22.12.21
Requires: smartmet-engine-grid >= 23.5.26
Requires: smartmet-server >= 23.5.19
Requires: %{smartmet_boost}-date-time
Requires: %{smartmet_boost}-filesystem
Requires: %{smartmet_boost}-iostreams
Requires: %{smartmet_boost}-system
Requires: %{smartmet_boost}-thread
Provides: %{SPECNAME}
#TestRequires: smartmet-utils-devel >= 23.4.28
#TestRequires: smartmet-library-spine-plugin-test >= 23.4.27
#TestRequires: smartmet-library-newbase-devel >= 23.2.9
#TestRequires: redis
#TestRequires: smartmet-test-db >= 23.6.1
#TestRequires: smartmet-test-data >= 23.5.15
#TestRequires: smartmet-engine-grid-test >= 23.5.26
#TestRequires: smartmet-library-gis >= 23.3.14
#TestRequires: smartmet-engine-geonames >= 23.4.27
#TestRequires: smartmet-engine-gis >= 22.12.21
#TestRequires: smartmet-engine-querydata >= 23.4.27
%if %{with observation}
#TestRequires: smartmet-engine-observation >= 23.6.1
%endif
#TestRequires: smartmet-engine-grid >= 23.5.26
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
%defattr(0664,root,root,0775)
%{_sysconfdir}/smartmet/plugins/edr/tmpl/*json

%changelog
* Wed May 24 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.5.24-1.fmi
- Configuration document added
- Parameter info in configuration file is prioritized over info from database

* Wed May 17 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.5.17-1.fmi
- Use VerticalProfile domain type in CoverageJSON output when there is one point, one timestep and several levels

* Thu May 11 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.5.11-1.fmi
- Improved observation metadata processing: Measurand info is fetched from database 
in order to get valid parameter list for each prodcer
- New configuration file entry 'visible_collections' added in order to limit number of collections in metadata

* Wed May 10 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.5.10-1.fmi
- Use latest obs engine API for looking for the latest observations

* Thu Apr 27 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.4.27-1.fmi
- Repackage due to macgyver ABI changes (AsyncTask, AsyncTaskGroup)

* Tue Apr 18 2023 Anssi Reponen <anssi.reponen@fmi.fi> -23.4.18-1.fmi
- Fixed API-document because of OGC test suite
- Fixed uninitalized variables and some other problems reported by CodeChecker

* Mon Apr 17 2023 Anssi Reponen <anssi.reponen@fmi.fi> -23.4.17-2.fmi
- Fixed handling of 'aviengine_disabled' flag
- Added configuration parameter 'avi.period_length' to tell valid period for METAR,TAF,SIGMET messages. Default is 30 days
- Added 'observation_period' parameter to tell valid period for observation engine collections (in hours). When running OGC test suite this should be relatively short, for example 24 hours
- temporal extent in metadata shows individual timesteps if there is less than 300 timesteps of varying length
- crs in metadata changed from EPSG:4326 to CRS:84, because ODG test suite required
- Fixed GeoJSON empty document
- Name and detail fields updated in locations metadata of AVI engine collections
- Configuration file 'url'-parameter used in addContentHandler function call (before hardcoded value was used)
- Fixed bbox format in cube-request
- Output files of test cases updated

* Mon Apr 17 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.4.17-1.fmi
- Repackaged due to GRID ABI changes

* Mon Mar 27 2023 Anssi Reponen <mika.heiskanen@fmi.fi> - 23.3.27-1.fmi
- Added title,description,keywords,crs fields to collection metadata (BRAINSTORM-2381)
- Show fields in the collection metadata in the following order: 1)id,2)title,3)description,4)links,5)output_formats,6)keywords,7)crs
- Made AVI-engine optional: 'aviengine_disabled' settings in configuration file and 'WITHOUT_AVI' blocks in source files

* Tue Mar 14 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.3.14-1.fmi
- Fixed format-option bug: application/json format allowed for metadata queries (BRAINSTORM-2565)
- Added API-definition pages (BRAINSTORM-2448)
- Added AVI tests (BRAINSTORM-2566)
- Enabled position- and radius-queries for AVI queries  (BRAINSTORM-2381)
- Make sure that collection id is unique, duplicate collections are removed (BRAINSTORM-2381)
- Added title to instances query  (BRAINSTORM-2381)

* Wed Mar  8 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.3.8-1.fmi
- Initialized POD data members properly
- Silenced CodeChecker warnings

* Mon Mar 6 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.3.6-1.fmi
- Added apikey to links
- Enabled corridor- and area-queries for AVI queries

* Mon Feb 20 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.2.20-1.fmi
- Support for GeoJSON format
- Support for METAR, TAF, SIGMET location queries
- Configuration file changed: output_formats-setting added
- Test results updated

* Thu Feb  9 2023 Pertti Kinnia <pertti.kinnia@fmi.fi> - 23.2.9-1.fmi
- Aviation message collection support (BRAINSTORM-2472)

* Wed Feb  8 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.2.8-1.fmi
- Add host name to stack traces

* Thu Jan 26 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.1.26-1.fmi
- Added support for request size limits (BRAINSTORM-2443)

* Thu Jan 19 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.1.19-1.fmi
- Repackaged due to ABI changes in grid libraries

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

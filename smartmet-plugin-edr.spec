%bcond_without observation
%define DIRNAME edr
%define SPECNAME smartmet-plugin-%{DIRNAME}
Summary: SmartMet edr plugin
Name: %{SPECNAME}
Version: 24.4.4
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
BuildRequires: smartmet-library-spine-devel >= 24.3.15
BuildRequires: smartmet-library-locus-devel >= 23.7.28
BuildRequires: smartmet-library-macgyver-devel >= 24.1.17
BuildRequires: smartmet-library-grid-content-devel >= 24.3.14
BuildRequires: smartmet-library-grid-files-devel >= 24.3.26
BuildRequires: smartmet-library-newbase-devel >= 24.3.11
BuildRequires: smartmet-library-gis-devel >= 24.3.25
BuildRequires: smartmet-library-timeseries-devel >= 24.2.23
BuildRequires: smartmet-engine-avi-devel >= 24.2.23
BuildRequires: smartmet-engine-geonames-devel >= 24.2.23
%if %{with observation}
BuildRequires: smartmet-engine-observation-devel >= 24.3.26
%endif
BuildRequires: smartmet-engine-querydata-devel >= 24.2.23
BuildRequires: smartmet-engine-gis-devel >= 24.2.23
BuildRequires: smartmet-engine-grid-devel >= 24.2.23
# obsengine can be disabled in configuration: not included intentionally
#%if %{with observation}
#Requires: smartmet-engine-observation >= 24.3.26
#%endif
Requires: fmt >= %{smartmet_fmt_min}, fmt < %{smartmet_fmt_max}
Requires: jsoncpp
Requires: smartmet-library-gis >= 24.3.25
Requires: smartmet-library-locus >= 23.7.28
Requires: smartmet-library-macgyver >= 24.1.17
Requires: smartmet-library-newbase >= 24.3.11
Requires: smartmet-library-spine >= 24.3.15
Requires: smartmet-library-timeseries >= 24.2.23
Requires: smartmet-library-gis >= 24.3.25
Requires: smartmet-library-grid-files >= 24.3.26
Requires: smartmet-engine-avi >= 24.2.23
Requires: smartmet-engine-geonames >= 24.2.23
Requires: smartmet-engine-querydata >= 24.2.23
Requires: smartmet-engine-gis >= 24.2.23
Requires: smartmet-engine-grid >= 24.2.23
Requires: smartmet-server >= 24.2.22
Requires: %{smartmet_boost}-date-time
Requires: %{smartmet_boost}-filesystem
Requires: %{smartmet_boost}-iostreams
Requires: %{smartmet_boost}-system
Requires: %{smartmet_boost}-thread
Provides: %{SPECNAME}
#TestRequires: smartmet-utils-devel >= 24.3.13
#TestRequires: smartmet-library-spine-plugin-test >= 24.3.15
#TestRequires: smartmet-library-newbase-devel >= 24.3.11
#TestRequires: redis
#TestRequires: smartmet-test-db >= 23.7.21
#TestRequires: smartmet-test-data >= 23.11.8
#TestRequires: smartmet-engine-grid-test >= 24.2.23
#TestRequires: smartmet-library-gis >= 24.3.25
#TestRequires: smartmet-engine-avi >= 24.2.23
#TestRequires: smartmet-engine-geonames >= 24.2.23
#TestRequires: smartmet-engine-gis >= 24.2.23
#TestRequires: smartmet-engine-querydata >= 24.2.23
%if %{with observation}
#TestRequires: smartmet-engine-observation >= 24.3.26
%endif
#TestRequires: smartmet-engine-grid >= 24.2.23
#TestRequires: gdal35
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
%{_datadir}/smartmet/edr/*json

%changelog
* Thu Apr  4 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.4.4-1.fmi
- Merged BRAINSTORM-2889 branch to master

* Fri Mar 22 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.3.22-2.fmi
- Use the same parameter names for 'parameters' and 'ranges'/'features' json elements; BRAINSTORM-2889

* Fri Mar 22 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.3.22-1.fmi
- Avoid string use for time value manipulations in GridInterface::prepareQueryTimes

* Mon Mar 11 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.3.11-1.fmi
- Fixed parameter name generation for parameters defined in configuration files

* Thu Feb 29 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.2.29-1.fmi
- Allow missing 'parameter-name' -option (BRAINSTORM-2874)

* Fri Feb 23 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> 24.2.23-1.fmi
- Full repackaging

* Thu Feb 22 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.2.22-1.fmi
- Repackaged due to ABI changes

* Wed Feb 21 2024 Mika Heiskanen <mheiskan@rhel8.dev.fmi.fi> - 24.2.21-1.fmi
- Removed support for obsolete bk_hydrometa producer

* Tue Feb 20 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> 24.2.20-1.fmi
- Repackaged due to grid-files ABI changes

* Mon Feb  5 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> 24.2.5-1.fmi
- Repackaged due to grid-files ABI changes

* Tue Jan 30 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> 24.1.30-1.fmi
- Repackaged due to newbase ABI changes

* Wed Jan 17 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> - 24.1.17-1.fmi
- Check size of bbox vector containing elements of splitted user given bbox string before referencing vector's fixed elements/indexes 0-3 to avoid crash if bbox string is invalid (BRAINSTORM-2839)
- Allow source data to have missing vertical extent when processing cube query for non grid source data (e.g. observations, surface querydata and aviation messages; BRAINSTORM-2827)
- Catch Fmi::stod() exception(s) when trying to convert string to double (BRAINSTORM-2824)

* Thu Jan  4 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.1.4-1.fmi
- Repackaged due to ABI changes in TimeSeriesGeneratorOptions

* Wed Jan  3 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.1.3-1.fmi
- Fixed TemporalCRS to TemporalRS (BRAINSTORM-2820)

* Fri Dec 22 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.12.22-1.fmi
- Repackaged due to ThreadLock ABI changes

* Tue Dec  5 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.12.5-1.fmi
- Repackaged due to an ABI change in SmartMetPlugin

* Mon Dec  4 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.12.4-1.fmi
- Improved EDR error messages in HTTP headers (BRAINSTORM-2812)

* Fri Nov 17 2023 Pertti Kinnia <pertti.kinnia@fmi.fi> - 23.11.17-1.fmi
- Repackaged due to API changes in grid-files and grid-content

* Thu Nov 16 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.11.16-1.fmi
- Repackaged since TimeSeries library RequestLimits ABI changed

* Fri Nov 10 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.11.10-1.fmi
- Silenced several compiler warnings on unnecessary copies

* Mon Oct 30 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.10.30-1.fmi
- Repackaged due to ABI changes in GRID libraries

* Sat Oct 21 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.10.21-1.fmi
- Added default value /usr/share/smartmet/edr for templatedir

* Thu Oct 19 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.10.19-1.fmi
- Move templates from /etc to /usr/share

* Wed Oct 18 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.10.18-1.fmi
- Locations for observations are fetched from database (not from geoengine) (BRAINSTORM-2750)
- Valid parameter list for observation producers can be defined in EDR configuration file (BRAINSTORM-2749)

* Thu Oct 12 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.10.12-1.fmi
- Moved some aggregation related functions to timeseries-library

* Wed Oct 11 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.10.11-1.fmi
- Moved some aggregation related functions to timeseries-library (BRAINSTORM-2457)

* Mon Oct  9 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.10.9-1.fmi
- Fixed api.json not to place an extra '/' in URIs
- Fixed home.json not to list a HTML link since it has not been implemented

* Thu Oct 5 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.10.5.fmi
- Fixed parameter name handling bug (BRAINSTORM-2746)

* Mon Oct  2 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.10.2-1.fmi
- Refactored GridInterface

* Fri Sep 29 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.9.29-1.fmi
- Repackaged due to changes in grid-libraries

* Thu Sep 28 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.9.28.fmi
- Code refactored (BRAINSTORM-2694)
- Json encoding fixed (BRAINSTORM-2732)
- Set parameter name in observedProperty.label field instead of measurement unit (BRAINSTORM-2732)
- IWXXM message tag changed from <avi> to <collect:meteorologicalInformation> (BRAINSTORM-2732)

* Mon Sep 11 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.9.11-1.fmi
- Repackaged due to ABI changes in grid-files

* Wed Aug 9 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.8.9-2.fmi
- Fixed AVI-engine metadata handling bug that crashed regression tests

* Wed Aug 9 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.8.9-1.fmi
- Preparations for new data notification of collections (BRAINSTORM-2613)

* Mon Jul 31 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.7.31-1.fmi
- Moved ImageFormatter to Spine

* Fri Jul 28 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.7.28-1.fmi
- Repackage due to bulk ABI changes in macgyver/newbase/spine

* Wed Jul 12 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.7.12-1.fmi
- Use postgresql 15, gdal 3.5, geos 3.11 and proj-9.0

* Tue Jul 11 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.7.11-1.fmi
- Repackaged due to QEngine API changes

* Tue Jun 13 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.13-1.fmi
- Support internal and environment variables in configuration files

* Wed Jun  7 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.7-1.fmi
- Fixed JSON strings to be properly encoded

* Tue Jun  6 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.6-1.fmi
- Repackaged due to GRID ABI changes

* Thu Jun  1 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.1-1.fmi
- New ObsEngine API with stationgroups setting forced a recompile

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

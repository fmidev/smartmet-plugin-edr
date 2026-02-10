%bcond_without observation
%define DIRNAME edr
%define SPECNAME smartmet-plugin-%{DIRNAME}
Summary: SmartMet edr plugin
Name: %{SPECNAME}
Version: 26.2.10
Release: 3%{?dist}.fmi
License: MIT
Group: SmartMet/Plugins
URL: https://github.com/fmidev/smartmet-plugin-edr
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

# https://fedoraproject.org/wiki/Changes/Broken_RPATH_will_fail_rpmbuild
%global __brp_check_rpaths %{nil}

%if 0%{?rhel} && 0%{rhel} < 9
%define smartmet_boost boost169
%else
%define smartmet_boost boost
%endif

%define smartmet_fmt_min 12.0.0
%define smartmet_fmt_max 13.0.0
%define smartmet_fmt fmt-libs >= %{smartmet_fmt_min}, fmt-libs < %{smartmet_fmt_max}
%define smartmet_fmt_devel fmt-devel >= %{smartmet_fmt_min}, fmt-devel < %{smartmet_fmt_max}

BuildRequires: rpm-build
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: %{smartmet_boost}-devel
BuildRequires: %{smartmet_fmt_devel}
BuildRequires: bzip2-devel
BuildRequires: zlib-devel
BuildRequires: libzip-devel
BuildRequires: jsoncpp-devel >= 1.8.4
BuildRequires: smartmet-library-spine-devel >= 26.2.4
BuildRequires: smartmet-library-locus-devel >= 26.2.4
BuildRequires: smartmet-library-macgyver-devel >= 26.2.4
BuildRequires: smartmet-library-grid-content-devel >= 26.2.4
BuildRequires: smartmet-library-grid-files-devel >= 26.2.4
BuildRequires: smartmet-library-newbase-devel >= 26.2.4
BuildRequires: smartmet-library-gis-devel >= 26.2.4
BuildRequires: smartmet-library-timeseries-devel >= 26.2.4
BuildRequires: smartmet-engine-avi-devel >= 26.2.4
BuildRequires: smartmet-engine-geonames-devel >= 26.2.4
%if %{with observation}
BuildRequires: smartmet-engine-observation-devel >= 26.2.4
%endif
BuildRequires: smartmet-engine-querydata-devel >= 26.2.4
BuildRequires: smartmet-engine-gis-devel >= 26.2.4
BuildRequires: smartmet-engine-grid-devel >= 26.2.4
# obsengine can be disabled in configuration: not included intentionally
#%if %{with observation}
#Requires: smartmet-engine-observation >= 26.2.4
#%endif
Requires: %{smartmet_fmt}
Requires: jsoncpp
Requires: smartmet-library-gis >= 26.2.4
Requires: smartmet-library-locus >= 26.2.4
Requires: smartmet-library-macgyver >= 26.2.4
Requires: smartmet-library-newbase >= 26.2.4
Requires: smartmet-library-spine >= 26.2.4
Requires: smartmet-library-timeseries >= 26.2.4
Requires: smartmet-library-gis >= 26.2.4
Requires: smartmet-library-grid-files >= 26.2.4
Requires: smartmet-engine-avi >= 26.2.4
Requires: smartmet-engine-geonames >= 26.2.4
Requires: smartmet-engine-querydata >= 26.2.4
Requires: smartmet-engine-gis >= 26.2.4
Requires: smartmet-engine-grid >= 26.2.4
Requires: smartmet-server >= 26.2.4
Requires: %{smartmet_boost}-filesystem
Requires: %{smartmet_boost}-iostreams
Requires: %{smartmet_boost}-system
Requires: %{smartmet_boost}-thread
Requires: libzip
Provides: %{SPECNAME}
#TestRequires: smartmet-utils-devel >= 26.2.4
#TestRequires: smartmet-library-spine-plugin-test >= 26.2.4
#TestRequires: smartmet-library-newbase-devel >= 26.2.4
#TestRequires: redis
#TestRequires: smartmet-test-db >= 26.2.4
#TestRequires: smartmet-test-data >= 25.8.13
#TestRequires: smartmet-engine-grid-test >= 26.2.4
#TestRequires: smartmet-library-gis >= 26.2.4
#TestRequires: smartmet-engine-avi >= 26.2.4
#TestRequires: smartmet-engine-geonames >= 26.2.4
#TestRequires: smartmet-engine-gis >= 26.2.4
#TestRequires: smartmet-engine-querydata >= 26.2.4
%if %{with observation}
#TestRequires: smartmet-engine-observation >= 26.2.4
%endif
#TestRequires: smartmet-engine-grid >= 26.2.4
#TestRequires: gdal312-libs
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
* Tue Feb 10 2026 Pertti Kinnia <pertti.kinnia@fmi.fi> 26.2.10-3.fmi
- Changed custom dimension interval to array of arrays (BRAINSTORM-3368). Converting querydata parameter descriptions from latin-1 to utf-8 (BRAINSTORM-3367)

* Tue Feb 10 2026 Pertti Kinnia <pertti.kinnia@fmi.fi> 26.2.10-2.fmi
- Setting value datatype 'float' if parameter's 1'st value is null since 'null' is not valid datatype (BRAINSTORM-3366)

* Mon Feb  9 2026 Pertti Kinnia <pertti.kinnia@fmi.fi> 26.2.9-1.fmi
- Fixes to response content type (BRAINSTORM-3267). Store custom dimension request parameter 'level' using 'custom_level' since 'level' is used internally to store 'z' parameter's values (BRAINSTORM-3349). Fixed/switched coordinate referencing axis order to x,y and accepted related test result chances (BRAINSTORM-3351). Changed coverage referencing coordinates system id scheme to http and accepted related test result chances (BRAINSTORM-3365)

* Wed Feb  4 2026 Andris Pavēnis <andris.pavenis@fmi.fi> 26.2.4-3.fmi
- Update to proj-9.7, gdal-3.12, fmt-12

* Wed Feb  4 2026 Pertti Kinnia <pertti.kinnia@fmi.fi> 26.2.4-2.fmi
- New release version. Fixed global data bbox buffering overflow in locations/all query (BRAINSTORM-3347). Removed irrelevant 'Missing parameter-name option' exception since the option is now optional

* Thu Jan 29 2026 Pertti Kinnia <pertti.kinnia@fmi.fi> 26.1.29-1.fmi
- Fixed bug resulting an unconfigured parameter or a configured parameter without standard_name to be included in query if parameter-name was not but some custom dimension(s) was given in query (PAK-6316)

* Wed Jan 28 2026 Pertti Kinnia <pertti.kinnia@fmi.fi> 26.1.28-1.fmi
- Added MetOcean profile custom dimension request parameters standard_name, method, duration and level (PAK-6316)

* Mon Jan 26 2026 Pertti Kinnia <pertti.kinnia@fmi.fi> 26.1.26-1.fmi
- Added custom dimension reference configuration and MetOcean profile custom dimensions to metadata (PAK-6315)

* Thu Jan 22 2026 Pertti Kinnia <pertti.kinnia@fmi.fi> 26.1.22-1.fmi
- Data response parameter descriptions and parameter, observedProperty and unit labels are stored using i18n (e.g. "en" : "Label") for the request language code (e.g. &lang=en) which defaults to configured language (PAK-6315)

* Tue Jan 20 2026 Pertti Kinnia <pertti.kinnia@fmi.fi> 26.1.20-1.fmi
- MetOcean profile related changes/additions to GeoJSON output (code copied from CoverageJSON output). Added missing measurementType output to CoverageJSON too

* Mon Jan 19 2026 Pertti Kinnia <pertti.kinnia@fmi.fi> 26.1.19-1.fmi
- Added configuration settings and changed CoverageJson metadata output according to (EDR core and) MetOcean profile requirements. If configuration for a parameter exists containing standard_name, level, method and/or duration, all of them are required. Parameter unit's label, symbol value and symbol type are now always required whether parameter's MetOcean related settings are present or not. Parameter name is used as the default for added parameter.label setting (it should contain english label and as such is the default for observed_property.label "en" label too) as well as for observed_property.label and parameter description as before (PAK-6315)
- Store collection metadata parameter description, observedProperty label and unit label as simple strings, not as objects having the value stored by the metadata language code

* Tue Jan 13 2026 Andris Pavēnis <andris.pavenis@fmi.fi> 26.1.13-1.fmi
- Repackage due to smartmet-engine-observation ABI changes

* Thu Jan  8 2026 Pertti Kinnia <pertti.kinnia@fmi.fi> 26.1.8-1.fmi
- Fixed zip source object allocation/releasing (BRAINSTORM-3331)

* Tue Dec 30 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> 25.12.30-1.fmi
- Fixed GIS library dependency

* Mon Dec 29 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> 25.12.29-1.fmi
- Repackaged due to API changes

* Fri Dec 19 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> 25.12.19-1.fmi
- Changed initial area buffering used by locations/all query which added ":1" to the end of the wkt due to crashes in gis -engine since it resulted to buffering value of 1000 degrees instead of meters. Now using the bbox by offsetting the corners by 0.1 degrees (BRAINSTORM-3328)

* Thu Dec 18 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> 25.12.18-1.fmi
- Changes to SIGMET query logic (BRAINSTORM-3325)
- Added special location 'all' to returned avi collection metadata (BRAINSTORM-3326)

* Tue Dec 16 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> 25.12.16-1.fmi
- Store avi collection station temporal extents too since they vary e.g. for taf -collection (BRAINSTORM-3320)

* Mon Dec 15 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> 25.12.15-1.fmi
- Do not check/filter in avi collection metadata query if messages were created before the given query / message time and do not limit query with publication time window for TAFs (BRAINSTORM-3321, BRAINSTORM-3300)

* Sun Dec 14 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> 25.12.14-1.fmi
- Various changes and fixes; e.g. added default output format and license configuration (BRAINSTORM-3315, BRAINSTORM-3312), returning http 204 on empty query result (BRAINSTORM-3317) and changes/fixes to merging and output of temporal extent period repeating interval (BRAINSTORM-3310, BRAINSTORM-3314)

* Tue Dec  9 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> 25.12.9-2.fmi
- Repackaged due to TimeSeries library API changes

* Tue Dec  9 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> 25.12.9-1.fmi
- Repackaged with older (not specified) libzip dependency since using packages from rhel-8-for-x86_64-appstream-rpms (has libzip-1.5.1-2) instead of remi-modular

* Mon Dec  8 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> 25.12.8-1.fmi
- New release version; changes needed by edr SWIM requirements (BRAINSTORM-3256) and MetOcean profile (PAK-6090)

* Mon Nov 24 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> 25.11.24-1.fmi
- Updated conformance specification (BRAINSTORM-3161)

* Wed Oct 29 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> 25.10.29-1.fmi
- Changed spatial extent bbox and vertical extent interval to array (BRAINSTORM-3268)

* Mon Oct 27 2025 Andris Pavēnis <andris.pavenis@fmi.fi> 25.10.27-1.fmi
- Update due to smartmet-library-spine ABI changes

* Wed Oct 15 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.10.15-1.fmi
- Repackaged due to grid-files API changes

* Wed Sep 10 2025 Andris Pavēnis <andris.pavenis@fmi.fi> 25.9.10-1.fmi
- Plugin::shutdown(): Ensure that metadata background update task is shut down

* Mon Sep  1 2025 Andris Pavēnis <andris.pavenis@fmi.fi> 25.9.1-2.fmi
- Update according to smartmet-library-spine ABI changes

* Fri Aug 22 2025 Andris Pavēnis <andris.pavenis@fmi.fi> 25.8.22-1.fmi
- Repackaged due to smartmet-engine-observation and smatrtmet-engine-avi changes

* Tue Aug 19 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.8.19-1.fmi
- Repackaged due to ObsEngine ABI changes

* Tue Aug  5 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> - 25.8.5-1.fmi
- More observation period fixes (BRAINSTORM-3017, BRAINSTORM-3116)

* Fri Aug  1 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> - 25.8.1-1.fmi
- Load sounding metadata from database and filter data by levels. Use vertical profile when outputting data (BRAINSTORM-3116)
- When no level is given in data request, use collection's metadata levels without min/max range check (BRAINSTORM-3223)

* Fri May  2 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> - 25.5.2-1.fmi
- Do not use 'format' request parameter to format table data since it's json formatted string and must be handled with ascii formatter

* Tue Apr  8 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.4.8-1.fmi
- Repackaged due to base library ABI changes

* Thu Apr  3 2025 Andris Pavēnis <andris.pavenis@fmi.fi> 25.4.3-1.fmi
- CoverageJson: format_output_data_point(): fix using std::vector values without checking of them presence

* Tue Apr  1 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> - 25.4.1-1.fmi
- Reverted order of z -axis values for pressure level data (BRAINSTORM-3157)

* Wed Mar 19 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.3.19-1.fmi
- Repackaged due to base library ABI changes

* Tue Mar  4 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> - 25.3.4-1.fmi
- Added 'license' link and missing 'links' to collection/instance/instanceid response (BRAINSTORM-3138)

* Tue Feb 18 2025 Andris Pavēnis <andris.pavenis@fmi.fi> 25.2.18-1.fmi
- Update to gdal-3.10, geos-3.13 and proj-9.5

* Fri Feb  7 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> - 25.2.7-1.fmi
- Fixed instance query to return given instance data instead of the latest one; BRAINSTORM-3131

* Fri Jan 24 2025 Pertti Kinnia <pertti.kinnia@fmi.fi> - 25.1.24-1.fmi
- Removed VerticalProfile response extra axis 't'; BRAINSTORM-3117

* Mon Jan 13 2025 Andris Pavēnis <andris.pavenis@fmi.fi> 25.1.13-1.fmi
- check that geonames location is available befor using it

* Thu Jan  9 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.1.9-2.fmi
- Repackaged due to GRID-library changes

* Thu Jan  9 2025 Andris Pavēnis <andris.pavenis@fmi.fi>
- Fix missing size check of boost::algorithm::split result before use

* Thu Jan  9 2025 Andris Pavēnis <andris.pavenis@fmi.fi> 25.1.9-1.fmi
- Fix missing size check of boost::algorithm::split result before use

* Thu Nov 28 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> - 24.11.28-1.fmi
- Added 'collect' namespace specification; BRAINSTORM-3086

* Fri Nov 22 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.11.22-1.fmi
- Aggregation update according to smartmet-library-timeseries ABI changes

* Fri Nov  8 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.11.8-1.fmi
- Repackage due to smartmet-library-spine ABI changes

* Wed Oct 23 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.10.23-1.fmi
- Repackaged due to ABI changes

* Wed Oct 16 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.10.16-1.fmi
- Repackaged due to ABI changes in grid libraries

* Fri Oct 11 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.10.11-1.fmi
- Moved custom dimension specification within extent specification (BRAINSTORM-3046)

* Fri Oct  4 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.10.4-1.fmi
- Fixed collection 'interval' to report only first and last time of the collection, not the start and end time of each period having different timestep. Also fixed the time format from range to single time value, start and end time are separate array items (BRAINSTORM-3042)
- Fixed collection interval 'values' to report first time instant as repeating interval start time when timestep changes, not the last time instant of the previous timestep (BRAINSTORM-3042)

* Mon Sep 16 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.9.16-1.fmi
- Fixed flash observation query result data vs edr query parameters indexing bug caused by special parameter 'data_source' (BRAINSTORM-3029)

* Tue Sep  3 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.9.3-1.fmi
- Added custom timestep dimension for 'instances' query too. Test result changes due to timestep dimension (BRAINSTORM-3013)
- Fixed bug in coverage json output; when fetching multiple querydata parameters, each parameter was handled as if it was the first (BRAINSTORM-3018)
- Fixed error message given if collection has no given parameters (BRAINSTORM-3019)

* Fri Aug 30 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.8.30-1.fmi
- Added custom timestep dimension (BRAINSTORM-3013)

* Fri Aug 16 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.8.22-1.fmi
- Fetch data using timestep=data (BRAINSTORM-3010)

* Fri Aug 16 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.8.16-1.fmi
- Fixed provider url at landing page (BRAINSTORM-3006)

* Mon Aug 12 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.8.12-1.fmi
- datetime parameter: do not allow use of durations (BRAINSTORM-2957)

* Wed Aug  7 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.8.7-1.fmi
- Update to gdal-3.8, geos-3.12, proj-94 and fmt-11

* Mon Jul 29 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.7.29-1.fmi
- Set correct mime type for IWXXM and TAC format output (BRAINSTORM-2961)

* Mon Jul 22 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.7.22-1.fmi
- Rebuild due to smartmet-library-macgyver changes

* Fri Jul 12 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.7.12-1.fmi
- Replace many boost library types with C++ standard library ones

* Wed Jun 12 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.6.12-1.fmi
- Added repeating interval support for z -parameter (BRAINSTORM-2976)

* Fri Jun  7 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.6.7-1.fmi
- Optimization: avoid using Fmi::format_time
- Ignore collections with invalid analysistime; filesys2smartmet uses analysistime embedded in file name

* Mon Jun  3 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.6.3-1.fmi
- Repackaged due to ABI changes

* Wed May 29 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.29-1.fmi
- Fix uninitialized shared pointers in previous release

* Tue May 28 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.28-1.fmi
- Remove uses of LocalTimePool

* Mon May 27 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.27-1.fmi
- Use prefix without trailing '/' and do not register separate handler for langing page

* Fri May 24 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.5.24-1.fmi
- Bug fixes to handle grid collection level data query (BRAINSTORM-2949)

* Thu May 16 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.5.16-1.fmi
- Validate instance id (BRAINSTORM-2933). Removed some leftover error checks never executed

* Tue May 14 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.5.14-1.fmi
- Do not accept 'intances' query for avi and observation collections (BRAINSTORM-2947)
- Register edr plugin baseurl with and without ending '/'; without to serve landing page e.g. at path /edr and with to block wildcard -like uri behaviour which resulted server to pass e.g. uri /edrmistake/collections to edr plugin (BRAINSTORM-2927)

* Mon May 13 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.5.13-1.fmi
- Changes/fixes for cube. Support 3d bbox. Validate bbox and z range/list/value. Using z -parameter instead of minz and maxz -parameters when querying with coords -parameter (BRAINSTORM-2925)
- Ensure configured edr base url starts with '/' but does not end with it. Added some validity checks for edr api url path and template configuration. Prefix edr api url paths with base url if they do not start with '/' (BRAINSTORM-2927)

* Tue May  7 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.7-1.fmi
- Use Date library (https://github.com/HowardHinnant/date) instead of boost date_time

* Fri May  3 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.5.3-1.fmi
- Repackaged due to GRID library changes

* Mon Apr 29 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.4.29-1.fmi
- Fixed bug in determining whether processing metatadata or data query (BRAINSTORM-2921)
- Set vertical profile 'axes' time axis outside of parameter/datavalue loop for the timestep on CoverageJson output. Setting it unnecessarily withing the parameter loop also resulted into extra '__uninitialized__' json child object for axes (BRAINSTORM-2900)
- Require 'coords' option with position, radius, area, trajectory and corridor queries. Cube query requires 'coords' or 'bbox' (BRAINSTORM-2924)
- Removed hardcoding for baseurl "/edr" (BRAINSTORM-2927)
- Require 'collections' to be the first uri keyword after basepath instead of just scanning for it when parsing the query (otherwise invalid paths having extra parts would be allowed) and fixed a bug when scanning for 'collections' in is_data_query() (fixes for BRAINSTORM-2927 and BRAINSTORM-2921)
- Handle query having both instance_id and location_id properly (BRAINSTORM-2922)

* Wed Apr 17 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.4.17-1.fmi
- Output specific collection instance as a collection (BRAINSTORM-2912)

* Mon Apr 15 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.4.15-1.fmi
- Output vertical profile as CoverageCollection (BRAINSTORM-2900)

* Fri Apr 12 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.4.12-1.fmi
- Use -1 (all significant digits) as default precision instead of 4 (BRAINSTORM-2896)

* Wed Apr 10 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.4.10-1.fmi
- By default JSON prettyprint is now off. Querystring option pretty can be used to override

* Fri Apr  5 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.4.5-1.fmi
- Added support for WIGOS wsi

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

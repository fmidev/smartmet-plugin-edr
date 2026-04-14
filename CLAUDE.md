# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

SmartMet EDR plugin (`smartmet-plugin-edr`) — implements the [OGC API - Environmental Data Retrieval](https://ogcapi.ogc.org/edr/) standard for SmartMet Server. Produces `edr.so` loaded dynamically by the server daemon.

## Build commands

```bash
make                    # Build edr.so
make clean              # Clean
make format             # clang-format (Google-based, Allman braces, 100-col)
make test               # Run all tests (sqlite + grid; oracle/postgresql if not CI/LOCAL_TESTS_ONLY)
make test-sqlite        # Run only sqlite-backed tests
make test-grid          # Run grid engine tests (requires Redis)
make test-oracle        # Run oracle-backed tests (needs DB access)
make test-postgresql    # Run postgresql-backed tests (needs DB access)
make rpm                # Build RPM
```

The build requires `REQUIRES = gdal configpp` via pkg-config and links against `libsmartmet-spine`, `libsmartmet-timeseries`, `libsmartmet-newbase`, `libsmartmet-locus`, `libsmartmet-gis`, `libsmartmet-macgyver`, `libsmartmet-grid-content`.

## Testing

Tests are integration tests using `smartmet-plugin-test` (not Boost.Test unit tests). The test harness starts a SmartMet server instance with the locally-built `edr.so`, sends HTTP requests from `test/*/input/*.get` files, and compares responses against `test/*/output/` expected results.

- **`test/base/`** — main EDR and timeseries-style tests against querydata/observation/avi engines (32 test cases). DB backend is selected via `@DB_TYPE@` substitution in reactor.conf.
- **`test/grid/`** — grid engine tests (10 test cases). Starts/stops a local Redis instance.
- Failures go to `test/*/failures/` directory for diffing against expected output.
- `.testignore_<dbtype>` files in `test/base/input/` skip tests not applicable to a given backend.

## Architecture

### Request flow

`Plugin::requestHandler` is the entry point. The plugin handles two distinct URL styles:
1. **EDR API requests** (`/edr/collections/...`) — parsed by `EDRQueryParams` which decodes the OGC EDR REST path into query type, collection, instance, coordinates, etc.
2. **TimeSeries-style requests** (optional, configured via `timeSeriesUrl`) — parsed by `TimeSeriesQuery`, providing backward-compatible timeseries plugin interface.

Both inherit from `CommonQuery` (which inherits from `ObsQueryParams`), sharing parameter/time/location parsing.

### Query dispatch

`QueryProcessingHub` routes data queries to the appropriate engine query handler based on the producer's source engine:
- **`QEngineQuery`** — querydata engine (FMI's native gridded format)
- **`ObsEngineQuery`** — observation engine (station measurements)
- **`GridEngineQuery`** — grid engine (GRIB/NetCDF via `GridInterface`)
- **`AviEngineQuery`** — aviation weather (METAR, TAC, IWXXM)

Metadata queries (`AllCollections`, `SpecifiedCollection`, `Instances`, `Locations`) are handled directly by `QueryProcessingHub` without dispatching to engine query handlers.

### EDR query types

Defined in `EDRQuery.h` as `EDRQueryType`: Position, Radius, Area, Cube, Trajectory, Corridor, Items, Locations, Instances.

### Output formats

- **CoverageJSON** (`CoverageJson.cpp`, ~3400 lines — the largest file) — primary EDR output format
- **GeoJSON** (`GeoJson.cpp`) — alternative spatial output
- IWXXM/TAC — aviation-specific XML/text formats (ZIP packaging supported)
- Standard timeseries formats via `Spine::TableFormatterFactory`

### Metadata system

`EngineMetaData` (wrapped in `Fmi::AtomicSharedPtr`) holds per-engine `EDRProducerMetaData` maps. Updated periodically by a background thread (`metaDataUpdateLoop`), configurable interval via `metaDataUpdateInterval`. The atomic shared pointer pattern ensures thread-safe reads during updates.

`EDRMetaData` contains spatial/temporal/vertical extents, parameter lists, and supported queries for each producer.

### Conditional compilation

`WITHOUT_OBSERVATION` and `WITHOUT_AVI` preprocessor guards allow building without observation or aviation engine dependencies. These are controlled by RPM spec `%bcond_without observation`.

### JSON handling

The plugin uses a custom `Json.h` wrapper (aliasing `Json::Value` etc.) rather than importing jsoncpp headers directly.

### Template files

`tmpl/*.json` — JSON templates for API landing page, conformance, and problem detail responses. Installed to `$(datadir)/smartmet/edr`.

### Key classes

- `Plugin` — main plugin class, owns engines, config, metadata, and the update loop
- `Config` — libconfig-based configuration (parsed from `edr.conf`), holds precisions, producer mappings, output format/query support per producer
- `State` — per-request immutable state, fixes wall clock time for query duration, caches querydata selections
- `Query` — EDR request (inherits both `EDRQueryParams` and `CommonQuery`)
- `CoordinateFilter` — filters output coordinates for spatial queries
- `Precision` — configurable numeric precision per parameter/producer

### Engine dependencies

Declared in `Engines.h` — the plugin obtains shared pointers to all engines at init time:
- `Engine::Querydata` — gridded weather data
- `Engine::Geonames` — location/geocoding
- `Engine::Gis` — coordinate/geometry operations
- `Engine::Grid` — GRIB/NetCDF grid data
- `Engine::Observation` — station observations (conditional)
- `Engine::Avi` — aviation weather (conditional)

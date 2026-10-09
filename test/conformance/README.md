# Conformance test suites

These tools run conformance test suites against a SmartMet Server with the locally built
`edr.so` and the test configuration of `test/base`. They need network access and are not
part of `make test`. `make test` runs `validate-outputs.py`, which checks the expected outputs
of the regression tests against the rules of OGC API-EDR and CoverageJSON.

```
make -C test conformance          # start the server, run both suites, stop the server
```

or step by step:

```
test/conformance/start-test-server.sh 8088
test/conformance/run-ogc-ets.sh http://localhost:8088/edr
test/conformance/metocean-checks.py http://localhost:8088/edr
test/conformance/stop-test-server.sh
```

The outputs are written to `test/conformance/tmp`.

## OGC API - EDR executable test suite

`run-ogc-ets.sh` runs the official OGC executable test suite
[ets-ogcapi-edr10](https://github.com/opengeospatial/ets-ogcapi-edr10) (EDR 1.0, the latest
released suite) from its all-in-one jar. The jar and a Java 17 runtime are downloaded to
`~/.cache/smartmet-edr-ets` with checksum verification, unless `JAVA` points to a java
executable. The suite reads the landing page, the API definition and up to `NOOFCOLLECTIONS`
(default 50) collections, and sends sample queries built from the collection metadata.

Known results with the test data:

* The query tests (position, area, trajectory, corridor) fail because the test data does not
  match the times the suite derives from the metadata: observation collections advertise the
  last 24 hours but the test database has old data (204 No Content), and the aviation
  collections have no temporal extent in the test data (`datetime=null`).
* The API definition test is skipped since the suite expects OpenAPI 3.0, the plugin
  serves OpenAPI 3.1 as required by the MetOcean profile.
* EDR GeoJSON (Features-like items) is not implemented.

## MetOcean profile checks

The [MetOcean profile of OGC API-EDR](https://github.com/opengeospatial/metocean-ogcapi-environmental-data-retrieval-profile)
has an abstract test suite only. `metocean-checks.py` implements the automatically testable
parts of it: the core class (identifiers, licenses, titles, CRSs, time reference system,
radius units, parameter metadata, locations, problem details), the data query response
format class (CoverageJSON support, spatial referencing ids), the NWP class (instances,
position and cube queries, 400 for a datetime between time steps) and the in-situ
observations class (custom dimensions, MetOcean parameter metadata). Collections are
classified by their identifiers, see `--nwp` and `--insitu` to choose them explicitly.

Many findings concern the configuration rather than the plugin: collection identifiers and
titles, license links, parameter units (QUDT, `parameter_info` in `edr.conf`), and CF
standard names of the observed properties. With the test configuration most parameters
have no unit since the configuration defines units only for a few parameters.

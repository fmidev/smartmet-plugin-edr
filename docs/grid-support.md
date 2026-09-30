# Grid support in the EDR plugin (developer notes)

These notes describe how the EDR plugin serves collections from the grid engine. They
complement the [programmer's tutorial](tutorial.md), which follows a request from URL to
CoverageJSON. For the grid services, see the developer guides of the
[grid engine](https://github.com/fmidev/smartmet-engine-grid/blob/master/docs/developer-guide.md)
and [grid-content](https://github.com/fmidev/smartmet-library-grid-content/blob/master/docs/developer-guide.md).

## Relation to the timeseries plugin

The EDR data path is a fork of the timeseries plugin's. `edr/GridEngineQuery.{h,cpp}`,
`edr/GridInterface.{h,cpp}` and `edr/QueryProcessingHub.cpp` correspond almost line by
line to the timeseries files of the same names (the EDR versions work on a
`CommonQuery` and fix a few spellings, `extractQueryResult()` instead of
`exteractQueryResult()`). Read the timeseries
[grid-support notes](https://github.com/fmidev/smartmet-plugin-timeseries/blob/master/docs/grid-support.md)
for how a request is routed to the grid engine (`source=grid`, grid producers,
`primaryForecastSource`, `defaultGridGeometries`), how the Query Server queries are built
per location and level, and how the results become time series. A fix in one plugin's
grid path usually needs the same change in the other.

## What is EDR-specific

### Collections from the grid engine

`get_edr_metadata_grid()` (`edr/EDRMetaData.cpp`) builds the grid collections from
`Engine::getEngineMetadata("")`. Each metadata record (producer, geometry, level type)
becomes one collection:

* **id** `PRODUCER.geometryId.levelId`, for example `ECG.1008.2`. The dots are how the
  plugin recognises a grid collection in a URL;
* **spatial extent** from the geometry's lat/lon corners;
* **temporal extent** from the analysis time (origin) and the available times;
* **vertical extent** from the levels, with the level type name and description as the
  VRS when there is more than one level;
* **parameters** from the parameter list, with names and units from the grid engine's
  mappings.

Records without times are skipped, and so are collections that
`visible_collections.<engine>` in the configuration hides. By default every grid
collection is visible.

### From collection to grid parameters

`EDRQueryParams` recognises a grid collection by the dot in its id and does **not** set
`producer=` for it. Instead, the id is split into producer, geometry id and level id,
and `handleGridParameter()` embeds them into every requested parameter
(`parameter:PRODUCER:geometryId:levelId:level`), so that the Query Server reads exactly
that producer, geometry and level type. The `z` value becomes the level. Grid pressure
levels are in **Pa** where querydata uses hPa, so `height-units` conversions differ for
grid collections (`hPa` is multiplied by 100 for grid, `Pa` divided by 100 for
querydata).

### Metadata refresh

The metadata snapshot is rebuilt periodically (see the tutorial). Grid collections
therefore appear and disappear with the content registry, one refresh interval later.

## Caching

As in timeseries, a producer group processed by the grid engine sets the product hash to
`Fmi::bad_hash` (`QueryProcessingHub.cpp`), so grid data responses have **no ETag** and
the frontend does not cache them.

## Pitfalls

* **Two copies of the same code.** The EDR and timeseries grid paths have already drifted
  apart in small ways. When you fix one, compare it with the other.
* **Collection ids depend on grid-files geometry and level ids.** Renumbering
  geometries in the grid-files configuration changes the public collection ids.
* **One collection per level type.** A producer with surface, pressure and model levels
  appears as three collections.

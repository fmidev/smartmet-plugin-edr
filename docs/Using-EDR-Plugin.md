
# How To Use the EDR API <!-- omit in toc -->

This page describes more indepth EDR API

- [Introduction](#introduction)
- [EDR metadata queries](#edr-metadata-queries)
  - [Metadata of all ollections](#metadata-of-all-collections)
  - [Metadata of specified collection](#metadata-of-specified-collection)
  - [Metadata of all instances of specified collection](#metadata-of-all-instances-of-specified-collection)
  - [Metadata of specified instance of specified collection](#metadata-of-specified-instance-of-specified-collection)
- [EDR data queries](edr-data-queries)
  - [Query types](query-types)
  - [Query paramaters](query-parameters)
  - [Position query](position-query)
  - [Radius query](radius-query)
  - [Area query](area-query)
  - [Cube query](cube-query)
  - [Trajectory query](trajectory-query)
  - [Corridor query](corridor-query)

## Introduction

EDR plugin implements EDR API as specified in OGC API - 'Environmental Data Retrieval Standard' document (https://ogcapi.ogc.org/edr/). The specification documens describes the API as follows *The Environmental Data Retrieval (EDR) Application Programming Interface (API) provides a family of lightweight query interfaces to access spatio-temporal data resources by requesting data at a Position, within an Area, along a Trajectory or through a Corridor.*

EDR plugin is used to retrieve small subsets from large collections of environmental data, such as meteorological forecasts or observations. The important aspect is that the data can be unambiguously specified by spatio-temporal coordinates.

The EDR plugin fetches observation data via ObsEngine module and the forecast data is fetched via the QEngine or GridEngine module. So, these modules should be in place when using the EDR plugin.

The following notations are used in the following chapters:

{host} = Base URI for the API server
{collectionId_} = an identifier for a specific collection of data
{instanceId_} = an identifier for a specific version or instance of a collection of data
{query_type} = an identifier for a specific query pattern to retrieve data from a specific collection of data

## EDR metadata queries

Metadata requests are used to retrieve metadata information about data provided by SmartMet server. Meatadata describes sptio-temporal attribues of each collection as well as supported query types.


### Metadata of all collections

Information of all available collections can be requested as follows:

```
{host}/edr/collections
```

Response, see Annex 1.

### Metadata of specified collection

Information of specified collection can be requested as follows:

```
{host}/edr/collections/{collectionId}
```
Currently one of the collections SmartMet provides is named ecmwf_eurooppa_mallipinta, so the request could be:

```
{host}/edr/collections/ecmwf_eurooppa_mallipinta
```
Response, see Annex 2.

### Metadata of all instances of specified collection

```
{host}/edr/collections/{collectionId}/instances
```

Currently one of the collections SmartMet provides is named ecmwf_eurooppa_mallipinta, so the request could be:

```
{host}/edr/collections/ecmwf_eurooppa_mallipinta/instances
```

### Metadata of specified instance of specified collection

```
{host_server}/edr/collections/{collectionId}/instances/{instanceId}
```
In data provided by FMI the {instanceId} is a timestamp, so the actual request could be for example:

```
{host}/edr/collections/ecmwf_eurooppa_painepinta/instances/20221103T000000
```

## EDR data queries

EDR data requests are used to retrieve spatio-temporal data from SmartMet server. The following path templates can be used in data requets

```
- /collections/{collectionId}/{queryType}
- /collections/{collectionId}/instances/{instanceId}/{queryType}
```

### Query types

The following query types are supported by EDR plugin.

| Path Template | Query Type | Description     |
| :---        |    :----   |          :--- |
| /collections/{collectionId}/position      | Position       | Return data for the requested position   |
| /collections/{collectionId}/radius      | Radius       | Return data within a given radius of a position   |
| /collections/{collectionId}/area      | Area       | Return data for the requested area   |
| /collections/{collectionId}/cube      | Cube       | Return data for a spatial cube   |
| /collections/{collectionId}/trajectory      | Trajectory       | Return data along a defined trajectory   |
| /collections/{collectionId}/corridor      | Corridor       | Return data within spatio-temporal corridor   |

### Query parameters

Query parameters are used in URLs to define the resources which are returned on a request. Some parameters are shared by all query types while the other are only relevant for some query types. The following are defined as standard shared parameters by all query types:

- coords
- datetime
- parameter-name

### Position query

The Position query returns data for the requested coordinate. Logic for identifying the best match for the coordinate will depend on
the collection . The filter constraints are defined by the following query parameters:

#### Parameter coords

Single  position:

```
POINT(x y)
```

List of positions:

```
MULTIPOINT((x y),(x1 y1),(x2 y2),(x3 y3))
```

List of positions at defined heights:

```
MULTIPOINTZ((x y z),(x1 y1 z1),(x2 y2 z2),(x3 y3 z3))
```

### Radius query

The Radius query returns data within the defined radius of the requested coordinate. The filter constraints are defined by the
following query parameters:

...

#### Parameter coords

### Area query

...

### Cube query

...

### Trajectory query

...

### Corridor query

...


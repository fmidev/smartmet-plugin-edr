
# How To Use the EDR API <!-- omit in toc -->

This page describes more indepth EDR API

- [Introduction](#introduction)
- [EDR metadata queries](#edr-metadata-queries)
  - [Metadata of all ollections](#metadata-of-all-collections)
  - [Metadata of specified collection](#metadata-of-specified-collection)
  - [Metadata of all instances of specified collection](#metadata-of-all-instances-of-specified-collection)
  - [Metadata of specified instance of specified collection](#metadata-of-specified-instance-of-specified-collection)
- [EDR data queries](#edr-data-queries)
  - [Query types](#query-types)
  - [Query paramaters](#query-parameters)
    - [Parameter coords](#parameter-coords---general)
    - [Parameter datetime](#parameter-datetime)
    - [Parameter parameter-name](#parameter-parameter-name)
    - [Parameter z](#parameter-z)
    - [Parameter crs](#parameter-crs)
    - [Parameter f](#parameter-f)
  - [Position query](#position-query)
    - [Parameter coords](#parameter-coords---position-query)
  - [Radius query](#radius-query)
    - [Parameter coords](#parameter-coords---radius-query)
    - [Parameter within and within-units](#parameter-within-and-within-units)
  - [Area query](#area-query)
    - [Parameter coords](#parameter-coords---area-query)
  - [Cube query](#cube-query)
    - [Parameter bbox](#parameter-bbox)
  - [Trajectory query](#trajectory-query)
      - [Parameter coords](#parameter-coords---trajectory-query)
  - [Corridor query](#corridor-query)
      - [Parameter coords](#parameter-coords---corridor-query)
  - [Example queries](#example-queries)

# Introduction

EDR plugin implements EDR API as specified in OGC API - 'Environmental Data Retrieval Standard' document (https://ogcapi.ogc.org/edr/). The specification documens describes the API as follows *The Environmental Data Retrieval (EDR) Application Programming Interface (API) provides a family of lightweight query interfaces to access spatio-temporal data resources by requesting data at a Position, within an Area, along a Trajectory or through a Corridor.*

EDR plugin is used to retrieve small subsets from large collections of environmental data, such as meteorological forecasts or observations. The important aspect is that the data can be unambiguously specified by spatio-temporal coordinates.

The EDR plugin fetches observation data via ObsEngine module and forecast data via QEngine or GridEngine module. So, these modules should be in place when using the EDR plugin.

The following notations are used in the following chapters:


- {host} = Base URI for the API server
- {collectionId} = an identifier for a specific collection of data
- {instanceId} = an identifier for a specific version or instance of a collection of data
- {query_type} = an identifier for a specific query pattern to retrieve data from a specific collection of data


# EDR metadata queries

Metadata requests are used to retrieve metadata information about data provided by SmartMet server. Meatadata describes sptio-temporal attribues of each collection as well as supported query types.


## Metadata of all collections

Information of all available collections can be requested as follows:

```
{host}/edr/collections
```

Response, see Annex 1.

## Metadata of specified collection

Information of specified collection can be requested as follows:

```
{host}/edr/collections/{collectionId}
```
Currently one of the collections SmartMet provides is named ecmwf_eurooppa_mallipinta, so the request could be:

```
{host}/edr/collections/ecmwf_eurooppa_mallipinta
```
Response, see Annex 2.

## Metadata of all instances of specified collection

```
{host}/edr/collections/{collectionId}/instances
```

Currently one of the collections SmartMet provides is named ecmwf_eurooppa_mallipinta, so the request could be:

```
{host}/edr/collections/ecmwf_eurooppa_mallipinta/instances
```

## Metadata of specified instance of specified collection

```
{host_server}/edr/collections/{collectionId}/instances/{instanceId}
```
In data provided by FMI the {instanceId} is a timestamp, so the actual request could be for example:

```
{host}/edr/collections/ecmwf_eurooppa_painepinta/instances/20221103T000000
```

# EDR data queries

EDR data requests are used to retrieve spatio-temporal data from SmartMet server. The following path templates can be used in data requets

```
- /collections/{collectionId}/{queryType}
- /collections/{collectionId}/instances/{instanceId}/{queryType}
```

## Query types

The following query types are supported by EDR plugin.

| Path Template | Query Type | Description     |
| :---        |    :----   |          :--- |
| /collections/{collectionId}/position      | Position       | Return data for the requested position   |
| /collections/{collectionId}/radius      | Radius       | Return data within a given radius of a position   |
| /collections/{collectionId}/area      | Area       | Return data for the requested area   |
| /collections/{collectionId}/cube      | Cube       | Return data for a spatial cube   |
| /collections/{collectionId}/trajectory      | Trajectory       | Return data along a defined trajectory   |
| /collections/{collectionId}/corridor      | Corridor       | Return data within spatio-temporal corridor   |

## Query parameters

Query parameters are used in URLs to define the resources which are returned on a request. Some parameters are shared by all query types while the other are only relevant for some query types. The following are defined as standard shared parameters by all query types:

- coords
- datetime
- parameter-name
- z
- crs
- f

### **Parameter coords - general**

Parameter _coords_ defines position(s) to return data for. The coordinates are defined by a Well Known Text (WKT) string.

Valid coords parameter depends on query type. See below valid values for parameter coords: [Position query](#position-query), [Radius query](#radius-query), [Area query](#area-query), [Trajectory query](#trajectory-query), [Corridor query](#corridor-query).

### **Parameter datetime**

Parameter _datetime_ defines timestamp(s) to return data for. Datetime can be a single timestamp or time interval.

_Example 1 - A single datetime_

```
datetime=2022-11-15T12:00:00Z
```
_Example 2 - Intervals_

November 15, 2022, 12:00:00 UTC to November 16, 2022, 12:00:00 UTC:
```
datetime=2022-11-15T12:00:00Z/2022-11-16T12:00:00Z
```
November 15, 2022, 12:00:00 UTC or later:
```
datetime=2022-11-15T12:00:00Z/..
```
November 15, 2022, 12:00:00 UTC or earlier:
```
datetime=../2022-11-15T12:00:00Z
```

### **Parameter parameter-name**

Parameter _parameter-name_ defines parameter(s) toreturn data for.

_Example 1 - A single parameter_

```
parameter-name=Temperature
```
_Example 2 - Multiple parameters_

```
parameter-name=Temperature,Pressure
```
### **Parameter z**

Parameter z defines vertical level(s) to return data from i.e. z=level. Lever may be for example pressure level or hybrid level.

_Example 1 - A single vertical level_

For example 850hPa pressure level is being queried:

```
z=850
```
_Example 2 - Return data at all levels defined by a list of vertical levels_

Request data at levels 1000hPa,900hPa,850hPa, and 700hPa:

```
z=1000,900,850,700
```

_Example 3 - Return data for all levels between and including 2 defined levels_

Request data for all hybrid levels between 60 and 110:

```
z=60/110
```
If level is not specified all available levels are returned.

### **Parameter crs**

Currently only EPSG:4326 is supported, and it is also default crs, so no need to use this parameter.

### **Parameter f**

Currently only CovergeJSON is supported, and it is also default format, so no need to use this parameter.

## Position query

The Position query returns data for the requested coordinate. Logic for identifying the best match for the coordinate will depend on
the collection . The filter constraints are defined by the following query parameters:

### **Parameter coords - Position query**

Single  position:

```
POINT(x y)
```

A  position at height z:

```
POINT(x y z)
```

List of positions:

```
MULTIPOINT((x y),(x1 y1),(x2 y2),(x3 y3))
```

List of positions at defined heights:

```
MULTIPOINTZ((x y z),(x1 y1 z1),(x2 y2 z2),(x3 y3 z3))
```

## Radius query

The Radius query returns data within the defined radius of the requested coordinate. The filter constraints are defined by the
following query parameters:

### **Parameter coords - Radius query**

The same coords variants are supported as in Position query.

### **Parameter within and within-units**

Paramneter within defines radius as plain number and within-units defined units.

For example define a radius of 20 Km from the position defined by the coords query parameter:

```
within=20&within-units=km
```
Within-units 'km' and 'mi' are supported.

## Area query

The Area query returns data within the polygon defined by the coords parameter. The height or time of the area are specified through separate parameters.

### **Parameter coords - Area query**

```
coords=POLYGON((x y,x1 y1,x2 y2,...​,xn yn, x y))
```
For instance a polygon that roughly describes an area that contains Southern Finland would look like:

```
coords=POLYGON((20.82 61.46,25.83 61.73,30.62 61.56,28.08 60.08,24.96 59.86,22.36 59.73,20.91 60.58,20.82 61.46))
```

Selecting data for two different regions. For instance a polygon that roughly describes an area that contains Southern Finland and Åland would look like:

```
coords=MULTIPOLYGON(((20.82 61.46,25.83 61.73,30.62 61.56,28.08 60.08,24.96 59.86,22.36 59.73,20.91 60.58,20.82 61.46)),((19.61 60.48,20.08 60.45,20.50 60.34,20.95 60.26,20.50 59.91,19.89 59.99,19.37 60.23,19.61 60.48)))
```

## Cube query

The Cube query returns a data cube defined by the bbox and z parameters.

### **Parameter bbox**

```
bbox=-6.0 50.0,-4.35 52.0
```

For instance a bbox that roughly describes an area that contains Southern Finland would look like

```
bbox=20.69 62.01,29.74 62.01,29.74 59.72,20.69 59.72,20.69 62.01
```

## Trajectory query

The Trajectory query returns data along the path defined by the *coords* parameter. 2D, 3D and 4D queries allowing the definition of a vertical level value (z) and a time value (as an epoch time) are supported, therefore coordinates for geometries may be 2D (x, y), 3D (x, y, z) or 4D (x, y, z, t).

### **Parameter coords - Trajectory query**

A 2D trajectory, on the surface of earth all at the same time and no vertical dimension, time value defined in ISO8601 format by the
datetime query parameter : 

```
coords=LINESTRING(-3.53 50.72, -3.35 50.92, -3.11 51.02, -2.85 51.42, -2.59 51.46)&datetime=2018-02-12T23:00:00Z
```

A 2D trajectory, on the surface of earth with no time value but at a fixed height level, height defined in the collection height units by
the z query parameter:

```
coords=LINESTRING(-3.53 50.72, -3.35 50.92, -3.11 51.02, -2.85 51.42, -2.59 51.46)&z=850
```

A 2D trajectory, on the surface of earth all at the same time and at a fixed height level, time value defined in ISO8601 format by the datetime query parameter and height defined in the collection height units by the z query parameter:

```
coords=LINESTRING(-3.53 50.72, -3.35 50.92, -3.11 51.02, -2.85 51.42, -2.59 51.46)&datetime=2018-02-12T23:00:00Z&z=850
```

A 3D trajectory, on the surface of the earth but over a range of time values with no height values:

```
coords=LINESTRINGM(-3.53 50.72 1560507000,-3.35 50.92 1560508800,-3.11 51.02 1560510600,-2.85 51.42 1560513600,-2.59 51.46 1560515400)
```

A 3D trajectory, on the surface of the earth but over a range of time values with a fixed vertical height value, height defined in the collection height units by the z query parameter:

```
coords=LINESTRINGM(-3.53 50.72 1560507000,-3.35 50.92 1560508800,-3.11 51.02 1560510600,-2.85 51.42 1560513600,-2.59 51.46 1560515400)&z=120
```

A 3D trajectory, through a 3D volume with vertical height, but no defined time:

```
coords=LINESTRINGZ(-3.53 50.72 80,-3.35 50.92 90,-3.11 51.02 100,-2.85 51.42 110,-2.59 51.46 120)
```

A 3D trajectory, through a 3D volume with height or depth, but at a fixed time value defined in ISO8601 format by the datetime query parameter:

```
coords=LINESTRINGZ(-3.53 50.72 0.1,-3.35 50.92 0.2,-3.11 51.02 0.3,-2.85 51.42 0.4,-2.59 51.46 0.5)&datetime=2018-02-12T23:00:00Z
```

A 4D trajectory, through a 3D volume and over a range of time values:

```
coords=LINESTRINGZM(-3.53 50.72 0.1 1560507000,-3.35 50.92 0.2 1560508800,-3.11 51.02 0.3 1560510600,-2.85 51.42 0.4 1560513600,-2.59 51.46 0.5 1560515400)
```

If the coords specify a 4D trajectory i.e. coords=LINESTRINGZM(...​) an error is  thrown by the server if the client application defines either the z or datetime query parameters.

The Z in LINESTRINGZ and LINESTRINGZM refers to the height value.The M in LINESTRINGM and LINESTRINGZM refers to the number of seconds that have elapsed since the Unix epoch, that is the time 00:00:00 UTC on 1 January 1970. See https://en.wikipedia.org/wiki/Unix_time

## Corridor query

The Corridor query returns data along and around the path defined by the *coords* parameter.

Currently EDR plugin supports 2D corridor queries 2D (x, y). The Linestring described by the coords parameter defines the center point of the corridor with corridor-width query parameter defining the breadth of the corridor.

### **Parameter coords - Corridor query**

A 2D corridor, on the surface of earth all at the same time and no vertical dimension, time value defined in ISO8601 format by the datetime query parameter:

```
coords=LINESTRING(-3.53 50.72,-3.35 50.92,-3.11 51.02,-2.85 51.42,-2.59 51.46)&datetime=2018-02-12T23:00:00Z
```

A 2D corridor, on the surface of earth all at a the same time and at a fixed vertical height, time value defined in ISO8601 format by the datetime query parameter and height defined in the collection height units by the z query parameter:

```
coords=LINESTRING(51.14 -2.98, 51.36 -2.87, 51.03 -3.15, 50.74 -3.48, 50.9 -3.36)&datetime=2018-02-12T23:00:00Z&z=850
```

## Example queries

**Example 1 - Position queries**

One point, one timestep:

```
/edr/collections/pal_skandinavia/position?coords=POINT(24.9384+60.1699)&datetime=200808051200&parameter-name=Temperature
 ```

 Multiple points, time interval:

```
/edr/collections/pal_skandinavia/position?coords=MULTIPOINT((24.9384+60.1699),(25.6653+60.3932))&datetime=200808051200/200808051800&parameter-name=Temperature
 ```

One point, time interval, pressure level 700:

```
 /edr/collections/ecmwf_skandinavia_painepinta/position?coords=POINT(24.9384+60.1699)&datetime=200809091200/200809091800&parameter-name=Temperature&z=700
 ```

**Example 2 - Radius queries**

Points within 20 km from defined coordinate:

```
/edr/collections/pal_skandinavia/radius?coords=POINT(24.9384+60.1699)&within=20&within-units=km&datetime=200808051200&parameter-name=Temperature
```

**Example 3 - Area queries**

```
/edr/collections/pal_skandinavia/area?coords=POLYGON((24+61,24+61.5,24.5+61.5,24.5+61,24+61))&datetime=200808051200&parameter-name=Temperature
```

**Example 4 - Cube queries**

Cube containing pressure levels 500 and 850:
```
/edr/collections/ecmwf_skandinavia_painepinta/cube?bbox=23.5+60.0,25.0+62.0&z=500,850&datetime=200809091200&parameter-name=Temperature
```
**Example 5 - Trajectory queries**

Trajectory, one timestep, pressure level 500:
```
/edr/collections/ecmwf_skandinavia_painepinta/trajectory?coords=LINESTRING(24.4839+60.9723,24.6864+61.0426,24.8562+61.1020,25.0196+61.0436,25.0196+60.9279,24.9738+60.8516)&datetime=200809091200&z=500&parameter-name=Temperature
```

Trajectory, different timestep for each coordinate, pressure level 1000:
```
 /edr/collections/ecmwf_skandinavia_painepinta/trajectory?coords=LINESTRINGM(24.4839+60.9723+1220961600,24.6864+61.04262008+1220972400,24.8562+61.1020+1220983200,25.0196+61.0436+1220994000,25.0196+60.9279+1221004800,24.9738+60.8516+1221015600)&z=1000&parameter-name=Temperature
 ```

Trajectory, one timestep, different pressure level for each coordinate:
```
 /edr/collections/ecmwf_skandinavia_painepinta/trajectory?coords=LINESTRINGZ(24.4839+60.9723+300,24.6864+61.0426+500,24.8562+61.1020+700,25.0196+61.0436+850,25.0196+60.9279+925,24.9738+60.8516+1000)&datetime=200809091200&parameter-name=Temperature
 ```

Trajectory, different timestep and pressure level for each coordinate:
```
 /edr/collections/ecmwf_skandinavia_painepinta/trajectory?coords=LINESTRINGZM(24.4839+60.9723+300+1220961600,24.6864+61.04262008+500+1220972400,24.8562+61.1020+700+1220983200,25.0196+61.0436+850+1220994000,25.0196+60.9279+925+1221004800,24.9738+60.8516+1000+1221015600)&parameter-name=Temperature
```

**Example 6 - Corridor queries**

One timestep, corridor width is 100 km, pressure level 500.

```
/edr/collections/ecmwf_skandinavia_painepinta/corridor?coords=LINESTRING(22.264824+60.454510,24.945831+60.192059,25.72088+62.24147,29.763530+62.601090)&corridor-width=100&width-units=km&datetime=200809091200&z=500&parameter-name=Temperature
```
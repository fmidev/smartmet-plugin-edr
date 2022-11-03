
# How To Use the EDR API <!-- omit in toc -->

This page describes more indepth EDR API

- [Introduction](#introduction)
- [EDR metadata queries](#edr-metadata-queries)
  - [Collections metadata](#collections-metadata)
  - [Collection instance metadata](#collection-instance-metadata)
- [EDR data queries]()

## Introduction

The EDR plugin offers a convenient way to fetch the meteorological time
series information over the Web using the HTTP protocol according to a specific
time or time interval chosen by the user. EDR plugin implements EDR API as specified in 'OGC API - Environmental Data Retrieval Standard' document (https://ogcapi.ogc.org/edr/)

EDR plugin is used to retrieve small subsets from large collections of environmental data, such as meteorological forecasts or observations. The important aspect is that the data can be unambiguously specified by spatio-temporal coordinates.

The EDR plugin fetches observation data via ObsEngine module and the forecast data is fetched via the QEngine or GridEngine module. So, these modules should be in place when using the TimeSeries plugin.

## EDR metadata queries

Metadata requests are used to retrieve information about meteorological data provided by SmartMet server.

### Collections metadata

Collections metadata query returns informaton of all available collections.

Request:

```
{host_server}/edr/collections
```

Response, see Annex 1.

### Collection instance metadata

Collection instance metadata query returns informaton of specified collection.

Request:

```
{host_server}/edr/collections/{collection_id}
```
Currently one of the collections SmartMet provides is named ecmwf_eurooppa_mallipinta, so the collection instance metadata request could be:

```
smartmet.fmi.fi/edr/collections/ecmwf_eurooppa_mallipinta
```
Response, see Annex 2.

## EDR data queries

...
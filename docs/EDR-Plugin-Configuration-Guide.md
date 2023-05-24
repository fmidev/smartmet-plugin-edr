
# EDR Plugin Configuration Guide <!-- omit in toc -->

This page describes more indepth EDR API

- [Introduction](#introduction)
- [Landing page](#landing-page)
- [AVI engine configuration](#avi-engine-configuration)
- [Supported data queries](#supported-data-queries)
- [Supported output formats](#supported-output-formats)
- [Supported locations](#supported-locations)
- [Disabling engines](#disabling-engines)
- [Visible collections](#visible-collections)
- [Collection info](#collection-info)
- [Parameter info](#parameter-info)
- [URL, language](#url-language)
- [Observation period](#observation-period)
- [Timeseries plugin settings](#timeseries-plugin-settings)



# Introduction

Name of EDR plugin configuration file is erd.conf.

# Landing page

EDR plugin landing page is defined as follows

```
api:
{
	templatedir	= "/home/reponen/work/smartmet/plugins/edr/tmpl";
        items:
        (
                {
			url = "/edr/";
			template = "home.json";
		},
		{
			url = "/edr/api";
			template = "api.json";
		},
		{
			url = "/edr/conformance";
			template = "conformance.json";
		}
        )
};
```

# AVI engine configuration

Because AVI engine does not provide enough information of its collections they must be introduced in configuration file. AVI engine collection are confgured as follows:

```
avi:
{
	period_length = 30; # 30 days
        collections:
        (
                {
                        name = "METAR";
                        countries = [ "FI" ];
                },
                {
                        name = "TAF";
                        countries = [ "FI" ];
                },
                {
                        name = "SIGMET";
                        icaos = [ "EFIN" ];
                        bbox:
                        {
                                xmin = 19;
                                ymin = 58;
                                xmax = 30;
                                ymax = 70;
                        }
                }
        )
};
```
- period_length parameter tells how old messages can be requested.


Note! if avi-section is not defined no avi collections are visible.


# Supported data queries

Supported data queries are configured as follows:

```
data_queries:
{
	default = ["position","radius","area","cube","locations","trajectory","corridor"];
	override:
	{
		METAR = ["position","radius","area","locations","trajectory","corridor"];
		TAF = ["position","radius","area","locations","trajectory","corridor"];
		SIGMET = ["position","radius","area","locations","trajectory","corridor"];
	};
};
```
The configuration above means that all other collections support 'position','radius','area','cube','locations','trajectory','corridor' queries except METAR,TAF and SIGMET collections which support 'position','radius','area','locations','trajectory','corridor' queries.


# Supported output formats

```
output_formats:
{
    default = ["CoverageJSON","GeoJSON"];
    override:
    {
        METAR = ["CoverageJSON","GeoJSON","IWXXM","TAC"];
        SIGMET = ["CoverageJSON","GeoJSON","IWXXM","TAC"];
        TAF = ["CoverageJSON","GeoJSON","IWXXM","TAC"];
    };
};
```

Currently all collections support CoverageJSON and GeoJSON output formats. Additionally METAR, TAF, SIGMET collections support IWXXM and TAC output formats.


# Supported locations

In locations section you can define a keyword that is used to get valid locations for each collection. EDR plugin uses this keyword to get locations from GeoEngine. These locations can then be used in location requests for querydata, observation, grid engine location requests. Note! AVI engine is an exception, it provides its own locations and they require no configuration.

```
locations:
{
#	 producer -> keyword
	default = ["synop_fi"];
	override:
	{
		mareograph = ["mareografit"];
	};
};
```

# Disabling engines

If you want to start EDR plugin without avi, observation, grid engine you can use the following settings.

```
aviengine_disabled    = true;
observation_disabled  = true;
gridengine_disabled   = true;
```

# Visible collections

EDR plugin shows all collectiosn provided by engines unless you define visible_collectins settings in configuration file and thus restrict the set of collections.

```
visible_collections:
{
        querydata_engine = ["pal_skandinavia","ecmwf_maailma_pinta"];
        observation_engine = ["road","opendata"];
        # Note! if entry not defined show all collections
        avi_engine = ["*"]; # all collections
        grid_engine = []; # no collections
};
```

# Collection info

Querydata, avi, observation engines do not provide enough information of their collections. In configuration file it is possible to define title, description, keywords for each collection. Note! Grid engine provides title, description information for its collections, so there is no need to configure grid collections.

```
collection_info:
{
	querydata_engine:
	(
		{
			id = "pal_skandinavia";
			title = "Title for collection pal_skandinavia";
			description = "Description for collection pal_skandinavia";
			keywords = ["pal_skandinavia"];
		},
                ...
	),
	avi_engine:
	(
                {
                        id = "METAR";
			title = "METAR message";
			description = "A METAR describes the current weather conditions at a location";
			keywords = ["METAR"];
                },
                ...
        ),
 	observation_engine:
	(
		{
			id = "airquality";
			title = "Title for collection airquality";
			description = "Description for collection airquality";
			keywords = ["airquality"];
		},
                ...
        )
}
```

# Parameter info

Parameter info presented in metadata is gathered from both from engine and configuration file. Configuration file is prioritized: If parameter info is defined in configuration file it is used, otherwise parameter information from engine is used. 

Parameter info in configuration file is defined as follows:

```
parameter_info:
{
	temperature:
	{
		description:
		{
			en = "Air temperature";
			fi = "Ilman lämpötila";
		};
		unit:
		{
			label:
			{
				en = "Celsius";
				fi = "Celsius";
			};
			symbol:
			{
				value = "˚C";
				type = "http://codes.wmo.int/common/unit/_degC";
			};
		};
	};
	...
};
```

# URL, language

```
url           = "/edr";
language      = "fi";
```

Effect of language setting can be seen in EDR metadata title and description fields of collections and parameters. Unfortunately, the language support is currently not comprehensive. Only the observation engine offers translations for parameters. Information about parameters can be added to the configuration file, in which case different language versions are also supported.

# Observation period

Observation database contains observations from very long period. If you want to restrict the period of observations that can be requested via EDR plugin you can define observation_period parameter. 

```
observation_period = 24; // hours
```

# Timeseries plugin settings

Since EDR plugin is build on timeseries plugin code base there are some common settings in configuration file, for example precision settings. For more information see timeseries plugin configuration guide: https://github.com/fmidev/smartmet-plugin-timeseries/blob/master/docs/Using-the-Timeseries-API.md#plugin-configuration-file
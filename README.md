# smartmet-plugin-edr

Part of [SmartMet Server](https://github.com/fmidev/smartmet-server). See the [SmartMet Server documentation](https://github.com/fmidev/smartmet-server) for a full overview of the ecosystem.

## Overview

The EDR plugin implements the [OGC API — Environmental Data Retrieval](https://ogcapi.ogc.org/edr/) standard for SmartMet Server. It provides a standardized REST interface for querying meteorological and environmental data at points, areas, and along trajectories.

In addition to the EDR API the plugin can optionally serve the TimeSeries API of
the SmartMet Server. The timeseries interface is enabled by defining the
`timeseries_url` attribute in the plugin configuration file; without it only the
EDR API is served.

## Features

- OGC API-EDR compliant interface
- Point, area, corridor, and trajectory queries
- JSON and CoverageJSON output formats
- Optional timeseries interface with the query syntax of the TimeSeries plugin

## Documentation

- [EDR Plugin Configuration Guide](docs/EDR-Plugin-Configuration-Guide.md)
- [Using the EDR Plugin](docs/Using-EDR-Plugin.md)

Timeseries interface (optional):

- [Using the Timeseries API](docs/Using-the-Timeseries-API.md)
- [Example Requests for Observations and Forecasts](docs/Timeseries-Examples.md)
- [Configuring the Timeseries Interface (Docker)](docs/Timeseries-Configuration-Docker.md)

## License

MIT — see [LICENSE](LICENSE)

## Contributing

Bug reports and pull requests are welcome on [GitHub](../../issues).

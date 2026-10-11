#!/usr/bin/env python3
"""
Executable checks for the MetOcean profile of OGC API - Environmental Data Retrieval
(https://github.com/opengeospatial/metocean-ogcapi-environmental-data-retrieval-profile).

The profile has an abstract test suite only, these checks implement the parts of it which can
be tested automatically against a running server.

Usage:
    metocean-checks.py [options] <landing page URL, e.g. http://localhost:8088/edr>

Options:
    --collections a,b      check only these collections (default: all)
    --nwp a,b|auto         collections checked against the NWP class (default: auto, i.e.
                           ids starting with weather_forecast and collections with instances)
    --insitu a,b|auto      collections checked against the in-situ observations class
                           (default: auto, i.e. ids starting with insitu-observations)
    --strict               treat SHOULD level findings (WARN) as failures

Each finding is printed as PASS, FAIL or WARN with the requirement id. The exit status is 1
if there are failures.
"""

import json
import re
import sys
import urllib.error
import urllib.parse
import urllib.request

results = []

QUDT_SYMBOLS = {"m", "km", "mi", "ft", "nmi", "cm", "mm", "dam", "hm"}
MEDIA_OPENAPI = {"application/openapi+json", "application/vnd.oai.openapi+yaml",
                 "application/vnd.oai.openapi+json"}
IDENTIFIERS = ("insitu-observations", "climate_data", "radar_observations", "weather_warnings",
               "weather_forecast")
RFC3339 = re.compile(r"^\d{4}-\d\d-\d\dT\d\d:\d\d(:\d\d(\.\d+)?)?(Z|[+-]\d\d:\d\d)$")


def report(status, req, target, message=""):
    results.append((status, req, target, message))


def check(condition, req, target, message, level="FAIL"):
    report("PASS" if condition else level, req, target, "" if condition else message)
    return condition


def get(url, accept="application/json"):
    """Returns (status, headers, body) without raising on HTTP errors"""
    request = urllib.request.Request(url, headers={"Accept": accept})
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            return response.status, dict(response.headers), response.read()
    except urllib.error.HTTPError as e:
        return e.code, dict(e.headers), e.read()


def get_json(url):
    status, headers, body = get(url)
    if status != 200:
        return status, None
    try:
        return status, json.loads(body)
    except ValueError:
        return status, None


def links_of(doc):
    return doc.get("links", []) if isinstance(doc, dict) else []


# ---------------------------------------------------------------------- Core


def check_openapi(base):
    req = "/req/core/openapi"
    status, landing = get_json(base + "/")
    if not check(landing is not None, req, "landing", f"landing page not available ({status})"):
        return
    desc = [l for l in links_of(landing) if l.get("rel") == "service-desc"]
    doc = [l for l in links_of(landing) if l.get("rel") == "service-doc"]
    ok_type = any(l.get("type", "").split(";")[0].strip() in MEDIA_OPENAPI for l in desc)
    check(ok_type, req + " B,C", "landing",
          "no service-desc link with an OpenAPI media type, got " +
          str([l.get("type") for l in desc]))
    check(any(l.get("type", "").startswith("text/html") for l in doc), req + " D", "landing",
          "no service-doc link of type text/html")
    if desc:
        status, api = get_json(urllib.parse.urljoin(base + "/", desc[0]["href"]))
        version = (api or {}).get("openapi", "")
        major_minor = tuple(int(x) for x in re.findall(r"\d+", version)[:2]) if version else (0, 0)
        check(major_minor >= (3, 1), req + " A", "api", f"OpenAPI version is '{version}', 3.1+ required")


def check_error_handling(base):
    req = "/req/core/error_handling C"
    status, headers, body = get(base + "/collections/no-such-collection-xyz")
    ctype = {k.lower(): v for k, v in headers.items()}.get("content-type", "")
    ok = 400 <= status < 500 and ctype.startswith("application/problem+json")
    if ok:
        try:
            problem = json.loads(body)
            ok = "title" in problem or "type" in problem
        except ValueError:
            ok = False
    check(ok, req, "unknown collection",
          f"expected a 4xx RFC 9457 problem details response, got {status} {ctype}", level="WARN")


def check_collection_core(cid, coll):
    target = cid
    # identifier
    check(cid.startswith(IDENTIFIERS), "/req/core/collection_identifier B", target,
          "identifier does not start with one of " + ", ".join(IDENTIFIERS), level="WARN")
    # license
    licenses = [l for l in links_of(coll) if l.get("rel") == "license"]
    check(bool(licenses), "/req/core/collection_license A,B", target, "no license link")
    if licenses:
        check(all(l.get("type") == "text/html" for l in licenses),
              "/req/core/collection_license C", target, "license link type is not text/html")
    # title
    title = coll.get("title", "")
    check(bool(title), "/req/core/collection_title A", target, "no title")
    if title:
        check(len(title) <= 50, "/req/core/collection_title B", target,
              f"title has {len(title)} characters, at most 50 allowed")
    # spatial extent
    spatial = coll.get("extent", {}).get("spatial", {})
    check(spatial.get("crs") == "OGC:CRS84", "/req/core/collection_spatial_extent A", target,
          f"extent.spatial.crs is '{spatial.get('crs')}', not OGC:CRS84")
    crs = coll.get("crs", [])
    check("OGC:CRS84" in crs, "/req/core/collection_spatial_extent B", target,
          f"crs {crs} does not include OGC:CRS84")
    for name, query in coll.get("data_queries", {}).items():
        details = query.get("link", {}).get("variables", {}).get("crs_details")
        if details is not None:
            check(any(d.get("crs") == "OGC:CRS84" for d in details),
                  "/req/core/collection_spatial_extent C", f"{target}/{name}",
                  "crs_details does not include OGC:CRS84")
    others = [c for c in crs if c != "OGC:CRS84"]
    check(all(re.match(r"^EPSG:\d+$", c) for c in others),
          "/req/core/collection_spatial_extent D", target,
          f"CRSs not in the EPSG:<code> form: {others}", level="WARN")
    # temporal extent
    temporal = coll.get("extent", {}).get("temporal")
    if temporal is not None:
        check(temporal.get("trs") == "Gregorian", "/req/core/collection_temporal_extent A", target,
              f"trs is '{temporal.get('trs')}', not Gregorian")
    # radius
    radius = coll.get("data_queries", {}).get("radius")
    if radius:
        units = radius.get("link", {}).get("variables", {}).get("within_units", [])
        check("m" in units, "/req/core/collection_radius_data_query A", target,
              f"within_units {units} does not contain m")
        check(all(u in QUDT_SYMBOLS for u in units), "/req/core/collection_radius_data_query B",
              target, f"within_units {units} are not all QUDT unit symbols", level="WARN")
    # parameters
    for pname, param in coll.get("parameter_names", {}).items():
        ptarget = f"{target}/{pname}"
        missing = [k for k in ("label", "description", "unit") if k not in param]
        check(not missing, "/req/core/collection_parameter_names B", ptarget,
              f"missing {missing}")
        label = param.get("label", "")
        if isinstance(label, str) and label:
            check(len(label) <= 50, "/req/core/collection_parameter_names C", ptarget,
                  f"label has {len(label)} characters")
        unit = param.get("unit")
        if isinstance(unit, dict):
            symbol = unit.get("symbol")
            stype = symbol.get("type", "") if isinstance(symbol, dict) else ""
            check(stype.startswith("https://qudt.org/vocab/unit/"),
                  "/req/core/collection_parameter_names E", ptarget,
                  f"unit.symbol.type '{stype}' is not a QUDT unit")
        op = param.get("observedProperty", {})
        check(str(op.get("id", "")).startswith("https://vocab.nerc.ac.uk/standard_name/") or
              "description" in op, "/req/core/collection_parameter_names F", ptarget,
              "observedProperty.id is not a CF standard name URI and there is no description",
              level="WARN")


def check_locations(base, cid, coll):
    if "locations" not in coll.get("data_queries", {}):
        return
    req = "/req/core/locations_query_response_format"
    status, doc = get_json(f"{base}/collections/{cid}/locations")
    if not check(doc is not None and doc.get("type") == "FeatureCollection", req + " A", cid,
                 f"locations response is not a GeoJSON FeatureCollection ({status})"):
        return
    bad = [f.get("id") for f in doc.get("features", [])
           if not isinstance(f.get("id"), str) or
           not isinstance(f.get("properties", {}).get("name"), str)]
    check(not bad, req + " B", cid, f"{len(bad)} features without string id and properties.name")


# ---------------------------------------------------------------------- Data queries


def sample_time(coll):
    temporal = coll.get("extent", {}).get("temporal") or {}
    interval = temporal.get("interval") or []
    if interval and isinstance(interval[0], list) and interval[0][0]:
        return interval[0][0]
    return None


def check_data_queries(base, cid, coll):
    queries = coll.get("data_queries", {})
    formats = coll.get("output_formats", [])
    for name, query in queries.items():
        if name in ("instances", "locations", "items"):
            continue
        qformats = query.get("link", {}).get("variables", {}).get("output_formats", formats)
        check("CoverageJSON" in qformats, "/req/data-query-response-format/encoding A",
              f"{cid}/{name}", f"output formats {qformats} do not include CoverageJSON")

    if "position" not in queries or "CoverageJSON" not in formats:
        return
    bbox = coll.get("extent", {}).get("spatial", {}).get("bbox", [[]])[0]
    if len(bbox) < 4:
        return
    params = list(coll.get("parameter_names", {}).keys())[:1]
    x = (bbox[0] + bbox[2]) / 2
    y = (bbox[1] + bbox[3]) / 2
    query = {"coords": f"POINT({x} {y})", "f": "CoverageJSON"}
    if params:
        query["parameter-name"] = params[0]
    t = sample_time(coll)
    if t:
        query["datetime"] = t
    for crs in [None] + [c for c in coll.get("crs", []) if c.startswith("EPSG:4326")]:
        q = dict(query)
        if crs:
            q["crs"] = crs
        url = f"{base}/collections/{cid}/position?" + urllib.parse.urlencode(q)
        status, headers, body = get(url, "application/vnd.cov+json")
        if status == 204:
            report("WARN", "/req/data-query-response-format/referencing", cid,
                   f"no data at the centre of the extent ({url})")
            continue
        try:
            doc = json.loads(body)
        except ValueError:
            doc = None
        if not check(status == 200 and doc is not None,
                     "/req/data-query-response-format/referencing", cid,
                     f"position query failed with {status}: {url}"):
            continue
        refs = doc.get("domain", {}).get("referencing") or doc.get("referencing") or []
        xy = [r for r in refs if "x" in r.get("coordinates", [])]
        system = xy[0].get("system", {}) if xy else {}
        expected = crs if crs else "OGC:CRS84"
        letter = "B" if crs else "A"
        check(system.get("type") == "GeographicCRS" and system.get("id") == expected,
              f"/req/data-query-response-format/referencing {letter}", cid,
              f"spatial referencing system is {system}, expected GeographicCRS {expected}")


# ---------------------------------------------------------------------- NWP and in-situ


def check_nwp(base, cid, coll):
    queries = coll.get("data_queries", {})
    check("position" in queries and "cube" in queries, "/req/nwp/collection_data_queries A", cid,
          "position and cube queries are not both supported")
    status, inst = get_json(f"{base}/collections/{cid}/instances")
    instances = (inst or {}).get("instances", [])
    if instances:
        bad = [i.get("id") for i in instances if not RFC3339.match(str(i.get("id", "")))]
        check(not bad, "/req/nwp/collection_granularity C", cid,
              f"instance ids are not RFC 3339 times: {bad[:3]}")
    values = (coll.get("extent", {}).get("temporal") or {}).get("values", [])
    m = re.match(r"^R\d+/([^/]+)/PT(\d+)([HM])$", values[0]) if values else None
    bbox = coll.get("extent", {}).get("spatial", {}).get("bbox", [[]])[0]
    if m and len(bbox) >= 4 and "position" in queries:
        start = m.group(1)
        step_minutes = int(m.group(2)) * (60 if m.group(3) == "H" else 1)
        if step_minutes > 1:
            # Half a time step after the first time
            import datetime
            t0 = datetime.datetime.strptime(start, "%Y-%m-%dT%H:%M:%SZ")
            t = (t0 + datetime.timedelta(minutes=step_minutes / 2)).strftime("%Y-%m-%dT%H:%M:%SZ")
            x = (bbox[0] + bbox[2]) / 2
            y = (bbox[1] + bbox[3]) / 2
            url = (f"{base}/collections/{cid}/position?" +
                   urllib.parse.urlencode({"coords": f"POINT({x} {y})", "datetime": t}))
            status, _, _ = get(url)
            check(status == 400, "/req/nwp/collection_data_queries C", cid,
                  f"a datetime between time steps gave {status}, not 400: {url}")


def check_insitu(cid, coll):
    custom = {c.get("id"): c for c in coll.get("extent", {}).get("custom", [])}
    req = "/req/insitu-observations/collection_custom_dimensions"
    check("standard_name" in custom, req + " A", cid, "no standard_name custom dimension")
    if "standard_name" in custom:
        check(custom["standard_name"].get("reference") == "https://vocab.nerc.ac.uk/standard_name/",
              req + " C", cid, "standard_name reference is not the NERC vocabulary")
    check("level" in custom, req + " D", cid, "no level custom dimension")
    queries = coll.get("data_queries", {})
    check(all(q in queries for q in ("locations", "area", "radius")),
          "/req/insitu-observations/collection_data_queries A", cid,
          "locations, area and radius are not all supported")
    for pname, param in coll.get("parameter_names", {}).items():
        missing = [k for k in ("metocean:standard_name", "metocean:level", "measurementType")
                   if k not in param]
        check(not missing, "/req/insitu-observations/collection_parameter_names", f"{cid}/{pname}",
              f"missing {missing}")


# ---------------------------------------------------------------------- main


def parse_list(value):
    return None if value in (None, "auto") else set(v for v in value.split(",") if v)


def main():
    args = sys.argv[1:]
    options = {}
    positional = []
    i = 0
    while i < len(args):
        if args[i] == "--strict":
            options["strict"] = True
            i += 1
        elif args[i].startswith("--"):
            options[args[i][2:]] = args[i + 1]
            i += 2
        else:
            positional.append(args[i])
            i += 1
    if len(positional) != 1:
        print(__doc__)
        return 2
    base = positional[0].rstrip("/")

    only = parse_list(options.get("collections"))
    nwp = parse_list(options.get("nwp", "auto"))
    insitu = parse_list(options.get("insitu", "auto"))

    check_openapi(base)
    check_error_handling(base)

    status, colls = get_json(base + "/collections")
    if colls is None:
        print(f"Failed to read {base}/collections ({status})")
        return 1

    for summary in colls.get("collections", []):
        cid = summary.get("id")
        if only and cid not in only:
            continue
        status, coll = get_json(f"{base}/collections/{cid}")
        if coll is None:
            report("FAIL", "/req/collections", cid, f"collection metadata not available ({status})")
            continue
        check_collection_core(cid, coll)
        check_locations(base, cid, coll)
        check_data_queries(base, cid, coll)
        is_nwp = (cid in nwp) if nwp is not None else (
            cid.startswith("weather_forecast") or
            any(l.get("rel") == "collection" and l.get("href", "").endswith("/instances")
                for l in links_of(coll)))
        if is_nwp:
            check_nwp(base, cid, coll)
        is_insitu = (cid in insitu) if insitu is not None else cid.startswith("insitu-observations")
        if is_insitu:
            check_insitu(cid, coll)

    counts = {"PASS": 0, "FAIL": 0, "WARN": 0}
    for status, req, target, message in results:
        counts[status] += 1
        if status != "PASS":
            print(f"{status}  {req}  {target}  {message}")
    print(f"\n{counts['PASS']} passed, {counts['FAIL']} failed, {counts['WARN']} warnings")
    failed = counts["FAIL"] + (counts["WARN"] if options.get("strict") else 0)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

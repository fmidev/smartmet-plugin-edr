#!/usr/bin/env python3
"""
Validate EDR plugin test outputs against the rules of OGC API-EDR 1.2 (19-086r9) and
CoverageJSON (21-069r2) that can be checked from a single response.

Usage: validate-outputs.py [--ignore <testignore-file>] <output-dir> [<output-dir> ...]

Checks every *.get and *.get.xz file which contains JSON (an optional "Status:" header
block produced with the X-Test-Headers request header is skipped). Exits with status 1
if any violations are found.
"""

import json
import lzma
import functools
import operator
import os
import re
import sys

errors = []


def error(fname, path, msg):
    errors.append(f"{fname}: {path}: {msg}")


def load(fname):
    opener = lzma.open if fname.endswith(".xz") else open
    with opener(fname, "rt", encoding="utf-8") as f:
        text = f.read()
    status = None
    if text.startswith("Status:"):
        header, _, text = text.partition("\n\n")
        status = int(header.split("\n")[0].split()[1])
    text = text.strip()
    if not text.startswith(("{", "[")):
        return None, status
    return json.loads(text), status


# ---------------------------------------------------------------------- CoverageJSON

AXIS_ORDER = ["t", "z", "y", "x", "composite"]


def axis_size(axis):
    if "values" in axis:
        return len(axis["values"])
    if all(k in axis for k in ("start", "stop", "num")):
        return axis["num"]
    return None


def check_domain(fname, path, domain, domain_type, has_referencing):
    if domain.get("type") != "Domain":
        error(fname, path, "domain type must be 'Domain'")
    axes = domain.get("axes")
    if not isinstance(axes, dict) or not axes:
        error(fname, path, "domain must have non-empty axes")
        return {}
    sizes = {}
    for name, axis in axes.items():
        size = axis_size(axis)
        if size is None:
            error(fname, f"{path}.axes.{name}", "axis must have values or start/stop/num")
            continue
        sizes[name] = size
        if axis.get("dataType") == "tuple":
            coords = axis.get("coordinates", [])
            for v in axis.get("values", []):
                if not isinstance(v, list) or len(v) != len(coords):
                    error(fname, f"{path}.axes.{name}", "tuple length must match coordinates")
                    break
        elif "dataType" in axis and axis["dataType"] == "primitive":
            error(fname, f"{path}.axes.{name}", "default dataType 'primitive' must be omitted")
    if not has_referencing and "referencing" not in domain:
        error(fname, path, "domain must have referencing")
    for i, ref in enumerate(domain.get("referencing", [])):
        check_referencing(fname, f"{path}.referencing[{i}]", ref)

    single = lambda a: sizes.get(a) == 1
    dt = domain_type
    if dt in ("Point", "PointSeries", "VerticalProfile"):
        for a in ("x", "y"):
            if not single(a):
                error(fname, path, f"{dt} domain must have a single-valued '{a}' axis")
    if dt == "Point":
        for a in ("z", "t"):
            if a in sizes and not single(a):
                error(fname, path, f"Point domain axis '{a}' must be single-valued")
    if dt == "PointSeries":
        if "t" not in sizes:
            error(fname, path, "PointSeries domain must have a 't' axis")
        if "z" in sizes and not single("z"):
            error(fname, path, "PointSeries domain axis 'z' must be single-valued")
    if dt == "VerticalProfile":
        if "z" not in sizes:
            error(fname, path, "VerticalProfile domain must have a 'z' axis")
        if "t" in sizes and not single("t"):
            error(fname, path, "VerticalProfile domain axis 't' must be single-valued")
    if dt == "Trajectory":
        comp = axes.get("composite", {})
        if comp.get("dataType") != "tuple" or comp.get("coordinates") not in (
            ["t", "x", "y"],
            ["t", "x", "y", "z"],
        ):
            error(fname, path, "Trajectory composite axis must be a (t,x,y[,z]) tuple")
        if "z" in sizes and not single("z"):
            error(fname, path, "Trajectory domain axis 'z' must be single-valued")
    if dt == "MultiPointSeries":
        if "composite" not in sizes or "t" not in sizes:
            error(fname, path, "MultiPointSeries domain must have 'composite' and 't' axes")
    if dt == "Grid":
        for a in ("x", "y"):
            if a not in sizes:
                error(fname, path, f"Grid domain must have an '{a}' axis")
    return sizes


def check_referencing(fname, path, ref):
    coords = ref.get("coordinates")
    system = ref.get("system")
    if not isinstance(coords, list) or not coords:
        error(fname, path, "referencing must have coordinates")
    if not isinstance(system, dict) or "type" not in system:
        error(fname, path, "referencing must have a system with a type")
        return
    t = system["type"]
    if t not in ("GeographicCRS", "ProjectedCRS", "VerticalCRS", "TemporalRS", "IdentifierRS"):
        error(fname, path, f"unknown reference system type '{t}'")
    if t == "TemporalRS" and "calendar" not in system:
        error(fname, path, "TemporalRS must have a calendar")
    if "id" in system and "TODO" in str(system["id"]):
        error(fname, path, "reference system id is a placeholder")
    if "description" in system and not isinstance(system["description"], dict):
        error(fname, path, "description must be an i18n object")


def check_parameters(fname, path, parameters):
    for name, p in parameters.items():
        ppath = f"{path}.{name}"
        if p.get("type") != "Parameter":
            error(fname, ppath, "parameter type must be 'Parameter'")
        op = p.get("observedProperty")
        if not isinstance(op, dict) or not isinstance(op.get("label"), dict):
            error(fname, ppath, "observedProperty must have an i18n label")
        for key in ("label", "description"):
            if key in p and not isinstance(p[key], dict):
                error(fname, ppath, f"{key} must be an i18n object")
        unit = p.get("unit")
        if unit is not None:
            if "label" not in unit and "symbol" not in unit:
                error(fname, ppath, "unit must have a label or a symbol")
            if "categories" in (op or {}):
                error(fname, ppath, "categorical parameter must not have a unit")


def check_ndarray(fname, path, arr, sizes):
    if arr.get("type") != "NdArray":
        error(fname, path, "range must be an NdArray")
        return
    values = arr.get("values")
    if not isinstance(values, list) or not values:
        error(fname, path, "NdArray values must be a non-empty array")
        return
    dtype = arr.get("dataType")
    if dtype not in ("float", "integer", "string"):
        error(fname, path, f"invalid dataType '{dtype}'")
    for v in values:
        if v is None:
            continue
        if dtype == "string" and not isinstance(v, str):
            error(fname, path, "non-string value in a string NdArray")
            break
        if dtype in ("float", "integer") and (isinstance(v, bool) or not isinstance(v, (int, float))):
            error(fname, path, f"non-numeric value {v!r} in a {dtype} NdArray")
            break
        if dtype == "integer" and isinstance(v, float) and not v.is_integer():
            error(fname, path, "non-integer value in an integer NdArray")
            break
    shape = arr.get("shape", [])
    names = arr.get("axisNames", [])
    if len(values) > 1 or shape:
        if len(shape) != len(names):
            error(fname, path, "shape and axisNames lengths differ")
        if functools.reduce(operator.mul, shape, 1) != len(values):
            error(fname, path, f"shape {shape} does not match {len(values)} values")
    for n, s in zip(names, shape):
        if n not in sizes:
            error(fname, path, f"axis '{n}' is not a domain axis")
        elif sizes[n] != s:
            error(fname, path, f"shape of axis '{n}' is {s}, domain has {sizes[n]} values")
    for n, s in sizes.items():
        if n not in names and s != 1:
            error(fname, path, f"multi-valued domain axis '{n}' missing from axisNames")
    order = [n for n in names if n in AXIS_ORDER]
    if order != sorted(order, key=AXIS_ORDER.index):
        error(fname, path, f"axisNames {names} not in the recommended order t, z, y, x")


def check_coverage(fname, path, cov, parent):
    if "domain" not in cov:
        error(fname, path, "coverage must have a domain")
        return
    domain = cov["domain"]
    if not isinstance(domain, dict):
        return
    domain_type = domain.get("domainType", cov.get("domainType", parent.get("domainType")))
    if parent.get("domainType") and ("domainType" in cov or "domainType" in domain):
        error(fname, path, "domainType should be omitted inside a collection that has one")
    sizes = check_domain(
        fname, path + ".domain", domain, domain_type, "referencing" in parent
    )
    params = cov.get("parameters", parent.get("parameters"))
    if params is None:
        error(fname, path, "coverage must have parameters")
        params = {}
    if "parameters" in cov:
        check_parameters(fname, path + ".parameters", cov["parameters"])
    ranges = cov.get("ranges")
    if not isinstance(ranges, dict):
        error(fname, path, "coverage must have ranges")
        return
    for name, arr in ranges.items():
        if name not in params:
            error(fname, f"{path}.ranges.{name}", "range has no parameter definition")
        check_ndarray(fname, f"{path}.ranges.{name}", arr, sizes)


def check_covjson(fname, doc):
    if doc["type"] == "Coverage":
        check_coverage(fname, "$", doc, {})
    else:
        if not isinstance(doc.get("coverages"), list):
            error(fname, "$", "coverage collection must have coverages")
            return
        for i, ref in enumerate(doc.get("referencing", [])):
            check_referencing(fname, f"$.referencing[{i}]", ref)
        if "parameters" in doc:
            check_parameters(fname, "$.parameters", doc["parameters"])
        for i, cov in enumerate(doc["coverages"]):
            check_coverage(fname, f"$.coverages[{i}]", cov, doc)


# ---------------------------------------------------------------------- EDR metadata

QUERY_TYPES = {"position", "radius", "area", "cube", "trajectory", "corridor", "items", "locations"}
WKT_KEYWORDS = ("GEOGCS", "PROJCS", "VERT_CS", "VERTCRS", "PARAMETRICCRS", "TIMECRS", "GEOGCRS")


def balanced(text):
    depth = 0
    for c in text:
        depth += {"[": 1, "]": -1}.get(c, 0)
        if depth < 0:
            return False
    return depth == 0


def check_links(fname, path, links, need_data):
    if not isinstance(links, list):
        error(fname, path, "links must be an array")
        return
    for i, link in enumerate(links):
        for key in ("href", "rel", "type"):
            if key not in link:
                error(fname, f"{path}[{i}]", f"link must have '{key}'")
    if need_data and not any(link.get("rel") in ("data", "instance") for link in links):
        error(fname, path, "links must include a link to a query or instance resource")


def check_collection(fname, path, coll):
    for key in ("id", "links", "extent", "data_queries", "crs", "output_formats", "parameter_names"):
        if key not in coll:
            error(fname, path, f"collection must have '{key}'")
    check_links(fname, path + ".links", coll.get("links", []), True)
    extent = coll.get("extent", {})
    spatial = extent.get("spatial", {})
    if "bbox" not in spatial or "crs" not in spatial:
        error(fname, path + ".extent.spatial", "spatial extent must have bbox and crs")
    crs = spatial.get("crs", "")
    if not (crs.startswith(WKT_KEYWORDS) or re.match(r"https?://www\.opengis\.net/def/crs/", crs)):
        error(fname, path + ".extent.spatial.crs", f"crs '{crs}' is neither WKT nor a CRS URI")
    for key, member in (("temporal", "trs"), ("vertical", "vrs")):
        if key in extent:
            text = extent[key].get(member, "")
            if not text.startswith(WKT_KEYWORDS) or not balanced(text):
                error(fname, f"{path}.extent.{key}.{member}", "must be a valid WKT definition")
    for name, q in coll.get("data_queries", {}).items():
        qpath = f"{path}.data_queries.{name}"
        if name not in QUERY_TYPES and name != "instances":
            error(fname, qpath, "unknown query type")
        link = q.get("link", {})
        for key in ("href", "rel", "variables"):
            if key not in link:
                error(fname, qpath, f"link must have '{key}'")
        variables = link.get("variables", {})
        if name in QUERY_TYPES and variables.get("query_type") != name:
            error(fname, qpath, "variables.query_type must match the query type")
        required = {"radius": ["within_units"], "cube": ["height_units"],
                    "corridor": ["width_units", "height_units"]}.get(name, [])
        for key in required:
            if not isinstance(variables.get(key), list):
                error(fname, qpath, f"variables must have a '{key}' string array")
        dof = variables.get("default_output_format")
        if dof and dof not in variables.get("output_formats", []) + coll.get("output_formats", []):
            error(fname, qpath, "default_output_format is not one of the output formats")
        for i, c in enumerate(variables.get("crs_details", [])):
            if "crs" not in c or "wkt" not in c:
                error(fname, f"{qpath}.crs_details[{i}]", "must have crs and wkt")
    for name, p in coll.get("parameter_names", {}).items():
        ppath = f"{path}.parameter_names.{name}"
        if p.get("type") != "Parameter":
            error(fname, ppath, "type must be 'Parameter'")
        op = p.get("observedProperty", {})
        if "label" not in op or "id" not in op:
            error(fname, ppath, "observedProperty must have id and label")


def check_edr_json(fname, doc, status):
    if "conformsTo" in doc:
        # The conformance class URIs of the OGC standards use the http scheme
        for c in ("http://www.opengis.net/spec/ogcapi-common-1/1.0/conf/core",
                  "http://www.opengis.net/spec/ogcapi-common-2/1.0/conf/collections"):
            if c not in doc["conformsTo"]:
                error(fname, "$.conformsTo", f"missing {c}")
        if not any(re.match(r"http://www\.opengis\.net/spec/ogcapi-edr-1/[0-9.]+/conf/core$", c)
                   for c in doc["conformsTo"]):
            error(fname, "$.conformsTo", "missing the EDR core conformance class")
    elif "detail" in doc and "title" in doc:
        if status is not None and doc.get("status") != status:
            error(fname, "$.status", f"problem detail status {doc.get('status')} != {status}")
    elif "collections" in doc:
        check_links(fname, "$.links", doc.get("links", []), False)
        ids = [c.get("id") for c in doc["collections"]]
        if len(ids) != len(set(ids)):
            error(fname, "$.collections", "collection ids must be unique")
        for i, c in enumerate(doc["collections"]):
            check_collection(fname, f"$.collections[{i}]", c)
    elif "instances" in doc:
        for i, c in enumerate(doc["instances"]):
            check_collection(fname, f"$.instances[{i}]", c)
    elif "data_queries" in doc:
        check_collection(fname, "$", doc)
    elif doc.get("type") == "FeatureCollection":
        if not isinstance(doc.get("features"), list):
            error(fname, "$", "FeatureCollection must have features")
        # Locations listings are metadata, data query responses carry links (A.83B)
        if "parameters" in doc:
            check_links(fname, "$.links", doc.get("links"), False)
    elif "links" in doc and "title" in doc:
        rels = {link.get("rel") for link in doc["links"]}
        # OGC API - Common uses the registered relation URIs, the short forms are also used
        for rel in ("conformance", "data"):
            if not rels & {rel, "http://www.opengis.net/def/rel/ogc/1.0/" + rel}:
                error(fname, "$.links", f"landing page must link to '{rel}'")
        if not rels & {"service-desc", "service-doc"}:
            error(fname, "$.links", "landing page must link to the API definition")


def main():
    # Tests ignored in this setup may have outdated expected outputs: --ignore <file>
    args = sys.argv[1:]
    ignored = set()
    while "--ignore" in args:
        i = args.index("--ignore")
        with open(args[i + 1]) as f:
            ignored |= {line.split("#")[0].strip() for line in f if line.split("#")[0].strip()}
        del args[i : i + 2]

    count = 0
    for directory in args:
        for root, _, files in os.walk(directory):
            # The timeseries style outputs are not EDR responses
            if os.path.basename(root) == "timeseries":
                continue
            for name in sorted(files):
                if not name.endswith((".get", ".get.xz")) or (name[:-3] if name.endswith(".xz") else name) in ignored:
                    continue
                fname = os.path.join(root, name)
                try:
                    doc, status = load(fname)
                except ValueError as e:
                    error(fname, "$", f"invalid JSON: {e}")
                    continue
                if not isinstance(doc, dict):
                    continue
                count += 1
                if doc.get("type") in ("Coverage", "CoverageCollection"):
                    check_covjson(fname, doc)
                else:
                    check_edr_json(fname, doc, status)
    for e in errors:
        print(e)
    print(f"Validated {count} JSON outputs, {len(errors)} violations")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())

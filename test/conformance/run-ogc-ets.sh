#!/bin/bash
#
# Runs the OGC API - EDR 1.0 executable test suite (ets-ogcapi-edr10) against an EDR server.
#
# Usage: run-ogc-ets.sh [landing page URL]      (default http://localhost:8088/edr)
#
# The suite jar (from Maven Central) and a Java 17 runtime (Eclipse Temurin) are downloaded to
# $ETS_CACHE (default ~/.cache/smartmet-edr-ets) unless JAVA points to a java executable.
# The checksums of the downloads are verified. The TestNG results are written to
# $WORKDIR/ets and summarized with ets-summary.py.

set -e

URL=${1:-http://localhost:8088/edr}
URL=${URL%/}
HERE=$(cd "$(dirname "$0")" && pwd)
WORKDIR=${WORKDIR:-$HERE/tmp}
ETS_CACHE=${ETS_CACHE:-$HOME/.cache/smartmet-edr-ets}
ETS_VERSION=${ETS_VERSION:-1.3}
NOOFCOLLECTIONS=${NOOFCOLLECTIONS:-50}

mkdir -p "$ETS_CACHE" "$WORKDIR"

JAR="$ETS_CACHE/ets-ogcapi-edr10-$ETS_VERSION-aio.jar"
if [ ! -f "$JAR" ]; then
  MAVEN=https://repo1.maven.org/maven2/org/opengis/cite/ets-ogcapi-edr10/$ETS_VERSION
  curl -sfL -o "$JAR.tmp" "$MAVEN/ets-ogcapi-edr10-$ETS_VERSION-aio.jar"
  expected=$(curl -sfL "$MAVEN/ets-ogcapi-edr10-$ETS_VERSION-aio.jar.sha1")
  actual=$(sha1sum "$JAR.tmp" | cut -d' ' -f1)
  if [ "$expected" != "$actual" ]; then
    echo "Checksum mismatch for the ETS jar" >&2
    rm -f "$JAR.tmp"
    exit 1
  fi
  mv "$JAR.tmp" "$JAR"
fi

if [ -z "$JAVA" ]; then
  JAVA=$(ls "$ETS_CACHE"/jre/*/bin/java 2> /dev/null | head -1)
  if [ -z "$JAVA" ]; then
    API='https://api.adoptium.net/v3'
    curl -sfL -o "$ETS_CACHE/jre.tgz" "$API/binary/latest/17/ga/linux/x64/jre/hotspot/normal/eclipse"
    expected=$(curl -sfL "$API/assets/latest/17/hotspot?architecture=x64&image_type=jre&os=linux&vendor=eclipse" |
                 python3 -I -c "import json,sys; print(json.load(sys.stdin)[0]['binary']['package']['checksum'])")
    actual=$(sha256sum "$ETS_CACHE/jre.tgz" | cut -d' ' -f1)
    if [ "$expected" != "$actual" ]; then
      echo "Checksum mismatch for the Java runtime" >&2
      exit 1
    fi
    mkdir -p "$ETS_CACHE/jre"
    tar xzf "$ETS_CACHE/jre.tgz" -C "$ETS_CACHE/jre"
    rm -f "$ETS_CACHE/jre.tgz"
    JAVA=$(ls "$ETS_CACHE"/jre/*/bin/java | head -1)
  fi
fi

# Note: the parameter names are case sensitive (apiDefinition) and the properties file must
# be given as a file: URI, otherwise it is looked up from the home directory
PROPS="$WORKDIR/ets-test-run-props.xml"
cat > "$PROPS" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE properties SYSTEM "http://java.sun.com/dtd/properties.dtd">
<properties version="1.0">
  <entry key="iut">$URL</entry>
  <entry key="apiDefinition">$URL/api</entry>
  <entry key="noofcollections">$NOOFCOLLECTIONS</entry>
</properties>
EOF

rm -rf "$WORKDIR/ets"
(cd "$WORKDIR" && "$JAVA" -jar "$JAR" -o "$WORKDIR/ets" "file:$PROPS" > "$WORKDIR/ets.log" 2>&1)
python3 "$HERE/ets-summary.py" "$(ls "$WORKDIR"/ets/testng/*/testng-results.xml | head -1)"

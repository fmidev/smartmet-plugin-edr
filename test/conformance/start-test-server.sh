#!/bin/bash
#
# Starts smartmetd with the locally built edr.so and the test configuration of test/base
# (local test geonames database, sqlite observations) for running conformance test suites.
#
# Usage: start-test-server.sh [port]      (default 8088)
#        stop-test-server.sh
#
# The server runs in the background, its log is in $WORKDIR/smartmetd.log.

set -e

PORT=${1:-8088}
HERE=$(cd "$(dirname "$0")" && pwd)
TESTDIR=$(cd "$HERE/.." && pwd)
WORKDIR=${WORKDIR:-$HERE/tmp}

mkdir -p "$WORKDIR/accesslog"

# The test database and the generated configuration files of test/Makefile
(cd "$TESTDIR" && CI=1 make cnf/avi.conf cnf/geonames.conf cnf/gis.conf geonames-database \
  > "$WORKDIR/database.log" 2>&1)

# Absolute paths since the configuration is not in the test directory
BASE="$TESTDIR/base"
CONF="$WORKDIR/smartmetd.conf"
{
  echo "port = $PORT;"
  sed -e "s:@DB_TYPE@:sqlite:g" \
      -e "s#\"edr.conf\"#\"$BASE/cnf/edr.conf\"#" \
      -e "s#\"../../../edr.so\"#\"$TESTDIR/../edr.so\"#" \
      -e "s#\"../../cnf/#\"$TESTDIR/cnf/#g" \
      -e "s#\"nogrid.conf\"#\"$BASE/cnf/nogrid.conf\"#" \
      -e "s#\"observation_sqlite.conf\"#\"$BASE/cnf/observation_sqlite.conf\"#" \
      "$BASE/cnf/reactor.conf.in"
} > "$CONF"

nohup smartmetd -c "$CONF" -p "$PORT" -a "$WORKDIR/accesslog" > "$WORKDIR/smartmetd.log" 2>&1 &
echo $! > "$WORKDIR/smartmetd.pid"

for _ in $(seq 1 60); do
  if curl -s -o /dev/null -f "http://localhost:$PORT/edr/collections"; then
    echo "SmartMet Server with the test configuration is running at http://localhost:$PORT/edr"
    exit 0
  fi
  sleep 2
done
echo "The server did not start, see $WORKDIR/smartmetd.log"
exit 1

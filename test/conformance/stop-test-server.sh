#!/bin/bash
#
# Stops the server started with start-test-server.sh and the test database

HERE=$(cd "$(dirname "$0")" && pwd)
WORKDIR=${WORKDIR:-$HERE/tmp}

if [ -f "$WORKDIR/smartmetd.pid" ]; then
  kill "$(cat "$WORKDIR/smartmetd.pid")" 2> /dev/null
  rm -f "$WORKDIR/smartmetd.pid"
fi
(cd "$HERE/.." && make stop-test-db > /dev/null 2>&1)
true

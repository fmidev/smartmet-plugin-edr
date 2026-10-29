include $(or $(PREFIX),$(shell smartbuildcfg --prefix))/share/smartmet/devel/makefile.inc

TEST_PREPARE_TARGETS := cnf/avi.conf cnf/geonames.conf cnf/gis.conf
TEST_FINISH_TARGETS := dummy

TEST_DB_DIR := $(shell pwd)/tmp-geonames-db

ifdef CI
GEONAMES_HOST_EDIT := sed -e 's|"smartmet-test"|"$(TEST_DB_DIR)"|g'
TEST_PREPARE_TARGETS += geonames-database
TEST_FINISH_TARGETS += stop-test-db
TEST_TARGETS := test-sqlite-impl test-grid validate-outputs validate-openapi
EXTRA_IGNORE := input/.testignore_circle-ci
else
ifdef LOCAL_TESTS_ONLY
GEONAMES_HOST_EDIT := cat
TEST_TARGETS := test-sqlite-impl test-grid validate-outputs validate-openapi
EXTRA_IGNORE := input/.testignore_circle-ci
else
GEONAMES_HOST_EDIT := cat
TEST_TARGETS := test-sqlite-impl test-oracle-impl test-postgresql-impl test-grid validate-outputs validate-openapi
EXTRA_IGNORE :=
endif
endif

TEST_RUNNER :=

all:

clean:
	-$(MAKE) $(TEST_FINISH_TARGETS)
	rm -rf failed-sqlite failed-oracle failed-postgresql
	rm -rf tmp-geonames-db
	rm -f stmp-geonames-db
#	Remove following line later
	rm -rf conf/grid

test:	$(TEST_PREPARE_TARGETS)
	ok=true; failed=; \
	for test in $(TEST_TARGETS); do \
          if ! $(MAKE) $$test; then ok=false; failed="$$failed $$test"; fi; \
	done; \
	if ! $$ok ; then \
	  echo "======================================================================="; \
	  echo "Failed: $$failed"; \
	  echo "======================================================================="; \
        fi; \
	$(MAKE) $(TEST_FINISH_TARGETS); \
	$$ok \

test-oracle test-postgresql test-sqlite: $(TEST_PREPARE_TARGETS)
	$(MAKE) $(@)-impl
	$(MAKE) $(TEST_FINISH_TARGETS);

test-oracle-impl test-postgresql-impl test-sqlite-impl:
	$(MAKE) -C base $(@:-impl=)

test-grid: $(TEST_PREPARE_TARGETS)
	$(MAKE) -C grid test-grid
	$(MAKE) $(TEST_FINISH_TARGETS)

# Check the expected outputs against the rules of OGC API-EDR and CoverageJSON. Tests not
# run in CI are skipped, their outputs may be outdated.
validate-outputs:
	if command -v python3 >/dev/null; then \
	  python3 validate-outputs.py --ignore base/input/.testignore_ci base/output grid/output; \
	else \
	  echo "python3 not available, skipping output validation"; \
	fi

# Conformance test suites against a local server, needs network access (see conformance/)
conformance:
	conformance/start-test-server.sh 8088
	ok=true; \
	conformance/run-ogc-ets.sh http://localhost:8088/edr > conformance/tmp/ogc-ets.txt || ok=false; \
	conformance/metocean-checks.py http://localhost:8088/edr > conformance/tmp/metocean.txt || ok=false; \
	conformance/stop-test-server.sh; \
	head -1 conformance/tmp/ogc-ets.txt; tail -1 conformance/tmp/metocean.txt; \
	$$ok

# Validate the generated OpenAPI definitions, and check that every successful test request is
# described by them (operation, enums, required and declared parameters)
validate-openapi:
	if command -v python3 >/dev/null; then \
	  python3 validate-openapi.py --ignore base/input/.testignore_ci \
	    base/output/openapi.get:base/input \
	    base/output/timeseries/openapi.get:base/input/timeseries; \
	else \
	  echo "python3 not available, skipping OpenAPI validation"; \
	fi

stmp-geonames-db:
	$(MAKE) geonames-database
	date >$@

geonames-database:
	$(MAKE) stop-test-db
	rm -rf tmp-geonames-db
	if ! /usr/share/smartmet/test/db/create-local-db.sh $(TEST_DB_DIR) >tmp-geonames-db.log 2>&1 ; then \
	    cat tmp-geonames-db.log; \
	    false; \
	fi
	/usr/share/smartmet/test/db/test-db-ctl.sh $(TEST_DB_DIR) start -w

stop-test-db:
	@test -f tmp-geonames-db/postmaster.pid &&\
		/usr/share/smartmet/test/db/test-db-ctl.sh $(TEST_DB_DIR) stop; \
		true

cnf/avi.conf cnf/geonames.conf cnf/gis.conf:
	$(GEONAMES_HOST_EDIT) $@.in >$@
	#diff -u $@.in $@; true

dummy:

.PHONY: cnf/geonames.conf cnf/gis.conf dummy validate-outputs validate-openapi conformance

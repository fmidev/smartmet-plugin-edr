include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

TEST_PREPARE_TARGETS := cnf/geonames.conf cnf/gis.conf
TEST_FINISH_TARGETS := dummy

TEST_DB_DIR := $(shell pwd)/tmp-geonames-db

ifdef CI
GEONAMES_HOST_EDIT := sed -e 's|"smartmet-test"|"$(TEST_DB_DIR)"|g'
TEST_PREPARE_TARGETS += geonames-database
TEST_FINISH_TARGETS += stop-test-db
TEST_TARGETS := test-sqlite-impl
EXTRA_IGNORE := input/.testignore_circle-ci
else
ifdef LOCAL_TESTS_ONLY
GEONAMES_HOST_EDIT := cat
TEST_TARGETS := test-sqlite-impl
EXTRA_IGNORE := input/.testignore_circle-ci
else
GEONAMES_HOST_EDIT := cat
TEST_TARGETS := test-sqlite-impl test-oracle-impl test-postgresql-impl
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

cnf/geonames.conf cnf/gis.conf:
	$(GEONAMES_HOST_EDIT) $@.in >$@

dummy:

.PHONY: cnf/geonames.conf cnf/gis.conf dummy

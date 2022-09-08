test-oracle:		DBTYPE := oracle
test-postgresql:	DB_TYPE := postgresql
test-sqlite:		DB_TYPE := sqlite

ifdef CI
TEST_TARGETS := test-sqlite
EXTRA_IGNORE := input/.testignore_ci
else
TEST_TARGETS := test
EXTRA_IGNORE :=
endif

test-sqlite test-oracle test-postgresql:
	rm -rf failures
	rm -rf tmp && mkdir tmp && cp output/* tmp/
	if grep --quiet "#define WGS84 1" /usr/include/smartmet/newbase/NFmiGlobals.h; then for f in tmp/*.wgs84; do mv $$f tmp/$$(basename $$f .wgs84); done; fi
	sed -e "s:@DB_TYPE@:$(DB_TYPE):g" cnf/reactor.conf.in >cnf/reactor.conf
	mkdir -p failures
	ok=true; $(TEST_RUNNER) smartmet-plugin-test --handler /edr/ -e tmp/ \
		--reactor-config cnf/reactor.conf \
		--failures-dir failures \
		$(foreach fn, input/.testignore_$(DB_TYPE) $(EXTRA_IGNORE), --ignore $(fn)) \
		--timeout 300 || ok=false; \
	$$ok

all:

clean:
	rm -rf failed cnf/reactor.conf

.PHONY: test

test:		DB_TYPE := oracle

ifdef CI
TEST_TARGETS := test-sqlite
EXTRA_IGNORE := input/.testignore_ci
else
TEST_TARGETS := test
EXTRA_IGNORE :=
endif

test:
	rm -rf failures
	rm -rf tmp && mkdir tmp && cp output/* tmp/
	if grep --quiet "#define WGS84 1" /usr/include/smartmet/newbase/NFmiGlobals.h; then for f in tmp/*.wgs84; do mv $$f tmp/$$(basename $$f .wgs84); done; fi
	mkdir -p failures
	ok=true; $(TEST_RUNNER) smartmet-plugin-test --handler /edr/ -e tmp/ \
		--reactor-config cnf/reactor_$(DB_TYPE).conf \
		--failures-dir failures \
		$(foreach fn, input/.testignore_$(DB_TYPE) $(EXTRA_IGNORE), --ignore $(fn)) \
		--timeout 300 || ok=false; \
	$$ok

all:

clean:
	rm -rf failed

.PHONY: test

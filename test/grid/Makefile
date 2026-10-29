include $(or $(PREFIX),$(shell smartbuildcfg --prefix))/share/smartmet/devel/makefile.inc

DB_TYPE := sqlite

# Starts the Redis test database listening only on a Unix socket below /tmp
GRID_TEST_REDIS ?= smartmet-grid-test-redis

all:

test test-grid:	start-redis-db
	if $(MAKE) DB_TYPE=$(DB_TYPE) test-impl; then ok=true; else ok=false; fi; $(MAKE) stop-redis-db; $$ok

clean:
	rm -rf failures failures-timeseries
	-$(GRID_TEST_REDIS) stop
	rm -rf cnf/grid

test-impl:
	ok=true; \
	$(TEST_RUNNER) \
	smartmet-plugin-test \
		--handler /edr \
		--reactor-config cnf/reactor.conf \
		$(foreach fn, input/.testignore_$(DB_TYPE), --ignore $(fn)) \
		--timeout 300 || ok=false; \
	$(TEST_RUNNER) \
	smartmet-plugin-test \
		--handler /timeseries \
		--reactor-config cnf/reactor.conf \
		--input-dir input-timeseries \
		--expected-output-dir output-timeseries \
		--failures-dir failures-timeseries \
		--timeout 300 || ok=false; \
	$$ok

start-redis-db:
	$(GRID_TEST_REDIS) start

stop-redis-db:
	$(GRID_TEST_REDIS) stop

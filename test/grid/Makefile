include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

DB_TYPE := sqlite

all:

test test-grid:	start-redis-db
	if $(MAKE) DB_TYPE=$(DB_TYPE) test-impl; then ok=true; else ok=false; fi; $(MAKE) stop-redis-db; $$ok

clean:
	rm -rf failures
	rm -rf cnf/grid

test-impl:
	ok=true; \
	smartmet-plugin-test \
		--handler /edr/ \
		--reactor-config cnf/reactor.conf \
		$(foreach fn, input/.testignore_$(DB_TYPE), --ignore $(fn)) \
		--timeout 300 || ok=false; \
	if [ -d input/timeseries ]; then \
	  smartmet-plugin-test --handler /timeseries \
		-e output/ \
		--reactor-config cnf/reactor.conf \
		$(foreach fn, input/.testignore_$(DB_TYPE), --ignore $(fn)) \
		--timeout 300 || ok=false; \
	fi; \
	$$ok

start-redis-db:
	rm -rf cnf/grid
	cp -pr $(datadir)/smartmet/test/grid cnf/
	@echo "*** Creating the Redis configuration file."
	smartmet-grid-test-config-creator cnf/environment.conf cnf/grid/redis/redis-template.conf cnf/grid/redis/redis.conf
	@echo "*** Starting the Redis database."
	@redis-server cnf/grid/redis/redis.conf

stop-redis-db:
	@if [ -f cnf/grid/redis/redis-server.pid ] ; then \
		cat cnf/grid/redis/redis-server.pid | xargs kill -9; \
		rm -f cnf/grid/redis/redis-server.pid; \
	fi

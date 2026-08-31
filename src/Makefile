EXTENSION = pg_cte_force
MODULES = pg_cte_force
DATA = pg_cte_force--0.0.1.sql

PG_CONFIG = pg_config

PG_MAJOR := $(shell $(PG_CONFIG) --version | sed -E 's/PostgreSQL ([0-9]+).*/\1/')

ifeq ($(shell test $(PG_MAJOR) -ge 14 && echo yes),)
$(error pg_cte_force requires PostgreSQL 14 or later)
endif

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
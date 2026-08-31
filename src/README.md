# pg_cte_force

PostgreSQL extension that globally forces the materialization behavior of CTEs (Common Table Expressions) that lack an explicit `MATERIALIZED` / `NOT MATERIALIZED` annotation.

Since PostgreSQL 12, CTEs are no longer materialized by default: the planner decides on a case-by-case basis unless the query explicitly specifies the desired behavior. `pg_cte_force` lets you override this decision globally (or per session) through a simple GUC parameter, without having to modify the text of existing queries.

## How it works

The extension installs a hook on `post_parse_analyze_hook`. After query analysis, a walker traverses the entire query tree (nested CTEs, subqueries, SubLinks, expressions) and, for every CTE that does **not** already have an explicit annotation, applies the current value of the `pg_cte_force.mode` GUC:

- if the CTE was written with `AS MATERIALIZED` or `AS NOT MATERIALIZED`, it is left untouched;
- otherwise its materialization behavior is set according to the active mode.

## GUC parameter

```
pg_cte_force.mode = default | materialized | not_materialized
```

| Value | Effect |
|---|---|
| `default` | No change: standard PostgreSQL behavior applies. |
| `materialized` | All CTEs without an explicit annotation are forced to `MATERIALIZED`. |
| `not_materialized` | All CTEs without an explicit annotation are forced to `NOT MATERIALIZED`. |

The parameter is `PGC_USERSET`, so it can be set per session, per user, per database, or in `postgresql.conf`.

## Installation

Requirements: `pg_config` available in `PATH` and the PostgreSQL development headers installed.

```bash
./build.sh
```

The script runs `make clean && make && make install`, copying `pg_cte_force.so` into PostgreSQL's `pkglibdir`.

Alternatively, manually:

```bash
make
sudo make install
```

Then, in a `psql` session:

```sql
CREATE EXTENSION pg_cte_force;
```

Note: the extension does not expose any SQL objects (functions, types, tables). The `pg_cte_force--1.0.sql` script exists only because `CREATE EXTENSION` requires one; all behavior is implemented in C and activated by `_PG_init()`.

## Loading the extension

Because `pg_cte_force` exposes no SQL objects, `CREATE EXTENSION` only
registers it in the catalog (useful for `pg_dump`/dependency tracking) — it
does **not** load the shared library into the backend, so by itself it does
not install the hook or activate the GUC. `_PG_init()` only runs once the
library is actually loaded, which happens in one of two ways:

- **Per session**, handy while developing (no server restart needed):

  ```sql
  LOAD 'pg_cte_force';
  ```

- **Cluster-wide**, loaded automatically for every new session:

  ```
  # postgresql.conf
  shared_preload_libraries = 'pg_cte_force'
  ```

  then restart the server. The `.so` must already be installed (`make
  install` / `build.sh`) *before* the restart, otherwise Postgres fails to
  start.

## Usage

```sql
-- Needed once per session unless pg_cte_force is already in
-- shared_preload_libraries (see "Loading the extension" above)
LOAD 'pg_cte_force';

-- Force all unannotated CTEs to be materialized
SET pg_cte_force.mode = 'materialized';

WITH t AS (SELECT * FROM big_table WHERE ...)
SELECT * FROM t JOIN other_table ON ...;

-- Restore PostgreSQL's default behavior
SET pg_cte_force.mode = 'default';
```

To make the setting persistent instead of session-scoped (this only takes
effect if `pg_cte_force` is also in `shared_preload_libraries` — otherwise
the value is stored but never read, since the hook is never installed):

```sql
-- Cluster-wide default (requires a config reload, e.g. SELECT pg_reload_conf();)
ALTER SYSTEM SET pg_cte_force.mode = 'materialized';

-- Per-database default (takes effect on new sessions connecting to that database)
ALTER DATABASE mydb SET pg_cte_force.mode = 'materialized';
```

## Project files

| File | Description |
|---|---|
| `pg_cte_force.c` | Extension source code: GUC, hook, and query tree walker. |
| `pg_cte_force--1.0.sql` | Empty SQL script required by `CREATE EXTENSION`. |
| `pg_cte_force.control` | Extension control file (version, description, relocatable). |
| `Makefile` | PGXS-based build. |
| `build.sh` | Convenience script for building and installing. |

## Compatibility

The code handles the differences in the `post_parse_analyze_hook` signature across PostgreSQL versions:

- **>= 19**: `JumbleState` passed as `const`;
- **>= 14 and < 19**: non-const `JumbleState`;
- **< 14**: hook without the `JumbleState` parameter.

## License

Not specified.

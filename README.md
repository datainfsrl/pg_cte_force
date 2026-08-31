# pg_cte_force - VM INSTALL

For the VM install please read file vm/README.md

# pg_cte_force — Docker development environment

Container to build/test `pg_cte_force` using the same toolchain, the same
`pg_config`, and the same Postgres server you'd get in production.

## Structure

```
.
├── Dockerfile
├── docker-compose.yml
├── init/                      <-- mounted at /docker-entrypoint-initdb.d in the container
│   ├── 001-shared.sh          (sets shared_preload_libraries and restarts Postgres)
│   └── 002-testdatabase.sql   (creates a "testdb" database with sample data)
└── src/                       <-- mounted at /extension in the container
    ├── pg_cte_force.c
    ├── pg_cte_force.control
    ├── pg_cte_force--1.0.sql
    ├── Makefile
    └── build.sh
```

The scripts in `init/` are executed by Postgres only once, the first time the
`pgdata` volume is initialized (in alphabetical order). They don't run again
on subsequent restarts of an already-initialized volume.

## Getting started

```bash
docker compose build
docker compose up -d
```

The Postgres server (default: version 19, see `PG_MAJOR` in
`docker-compose.yml`) starts right away with user `postgres` / password
`devpass`, database `extdev`, exposed on `localhost:5433`.

On the first run against an empty `pgdata` volume, the scripts in `init/`
run automatically: `001-shared.sh` already sets
`shared_preload_libraries = 'pg_cte_force'` and restarts the server, while
`002-testdatabase.sql` creates a `testdb` database with a sample table
(`test_table`, 1000 rows, indexed on `name`). If you're reusing an existing
`pgdata` volume, these scripts are skipped.

### 1. Manual per-session load (fast iteration, no restart)

```sql
LOAD 'pg_cte_force';
SET pg_cte_force.mode = 'materialized';
EXPLAIN (COSTS OFF)
WITH x AS (SELECT * FROM tbl WHERE ...) SELECT * FROM x JOIN ...;
```

Handy while you're changing the code: rebuild, reopen the psql session,
`LOAD` again. No server restart needed.

### 2. shared_preload_libraries ("production" behavior)

On a freshly created `pgdata` volume this is already done automatically by
`init/001-shared.sh` (see above). The steps below are only needed if you're
working on an existing volume, or want to redo this manually:

```sql
ALTER SYSTEM SET shared_preload_libraries = 'pg_cte_force';
```

then, from outside the container:

```bash
docker compose restart pg-dev
```

At that point:

```sql
ALTER DATABASE extdev SET pg_cte_force.mode = 'not_materialized';
```

applies to every new connection to `extdev`, with no need for `LOAD`.

⚠️ Watch the ordering: if you set `shared_preload_libraries` in
`postgresql.conf` (or via `ALTER SYSTEM`) **before** ever having run
`make install`, the Postgres restart fails because the `.so` file doesn't
exist yet. Always build the extension at least once before enabling the
preload.

If you'd rather isolate the agent from root, consider adding a dedicated
`dev` user to the Dockerfile and running
`docker compose exec -u dev pg-dev bash` for interactive sessions (the
Postgres server itself stays managed by the `postgres` user as in the
original image).

## Testing against another major version

`PG_MAJOR` selects which `postgresql-server-dev-*` headers get installed;
the base image itself is picked by `PG_VERSION` (the official `postgres`
image tag, e.g. `14`, `17`, or `19beta3` while 19 is still in beta). To test
a different major version you need to override both:

```bash
docker compose build --build-arg PG_MAJOR=14 --build-arg PG_VERSION=14
docker compose up -d
```

Note: since the `pgdata` Docker volume holds a data directory that's
incompatible across major versions, switching versions is best done from a
clean volume:

```bash
docker compose down -v
docker compose build --build-arg PG_MAJOR=14 --build-arg PG_VERSION=14
docker compose up -d
```

## Note on the data layout (Postgres 18+)

Starting with version 18, the official `postgres` images changed: data must
be mounted at `/var/lib/postgresql` (no longer at
`/var/lib/postgresql/data`), because they now use a directory layout
compatible with `pg_ctlcluster` (per-major-version subdirectories, to
support `pg_upgrade --link` without mount-point issues). The
`docker-compose.yml` in this repo is already set up correctly for PG18+.

If you're migrating from an earlier version of this setup (or from a volume
created with a Postgres image <18), the typical error is:

```
Error: in 18+, these Docker images are configured to store database data in a
       format which is compatible with "pg_ctlcluster" ...
       Counter to that, there appears to be PostgreSQL data in:
         /var/lib/postgresql/data (unused mount/volume)
```

Solution: the old volume can't be reused in the new format, it needs to be
recreated:

```bash
docker compose down -v
docker compose up -d
```

If instead you explicitly need to test a version < 18 (e.g. `PG_MAJOR=14`),
the traditional mount at `/var/lib/postgresql/data` is fine for those
images — but since this compose file defaults to PG18+, remember to check
the correct path for the specific image you're using if you go lower.

# INSTALL ON a VM instance

## Example for PostgreSQL 18 on UBUNTU 24.04 noble

```bash

sudo apt install -y postgresql-common

sudo /usr/share/postgresql-common/pgdg/apt.postgresql.org.sh

sudo apt-get update && sudo apt-get install -y --no-install-recommends \
    build-essential \
    postgresql-server-dev-18 \
    postgresql-18 \
    git \
    curl \
    ca-certificates \
    gnupg \
    less \
    sudo \
    locales \
    && sudo rm -rf /var/lib/apt/lists/*


git clone https://github.com/datainfsrl/pg_cte_force.git

cd pg_cte_force/src

sudo su

./build.sh


```

## Configuring PostgreSQL

add to postgresql.conf

```bash
shared_preload_libraries = 'pg_cte_force'
```

and restart PostgreSQL.

```bash
systemctl restart postgresql
```

## Testing the extension

```sql
create database testdb;
\c testdb;
create table test_table as (select generate_series(1, 1000) as id, 'test' || generate_series(1, 1000)::text as name);
create index idx_test_table_name on test_table(name);
alter database testdb set pg_cte_force.mode = 'materialized';
```

Reconnecting to the testdb (or you can you also the set pg_cte_force.mode = 'materialized' )

Here below some tests:

```bash
postgres@dev:~$ psql testdb
psql (18.6 (Ubuntu 18.6-1.pgdg24.04+2))
Type "help" for help.

testdb=# show pg_cte_force.mode ;
 pg_cte_force.mode
-------------------
 materialized
(1 row)

testdb=# explain with x as (select * from test_table ) select * from x limit 1;
                              QUERY PLAN
-----------------------------------------------------------------------
 Limit  (cost=18.00..18.02 rows=1 width=36)
   CTE x
     ->  Seq Scan on test_table  (cost=0.00..18.00 rows=1000 width=11)
   ->  CTE Scan on x  (cost=0.00..20.00 rows=1000 width=36)
(4 rows)


testdb=# explain with x as not materialized (select * from test_table ) select * from x limit 1;
                             QUERY PLAN
---------------------------------------------------------------------
 Limit  (cost=0.00..0.02 rows=1 width=11)
   ->  Seq Scan on test_table  (cost=0.00..18.00 rows=1000 width=11)
(2 rows)


testdb=# explain with x as materialized (select * from test_table ) select * from x limit 1;
                              QUERY PLAN
-----------------------------------------------------------------------
 Limit  (cost=18.00..18.02 rows=1 width=36)
   CTE x
     ->  Seq Scan on test_table  (cost=0.00..18.00 rows=1000 width=11)
   ->  CTE Scan on x  (cost=0.00..20.00 rows=1000 width=36)
(4 rows)



testdb=# set pg_cte_force.mode = 'default';
SET
testdb=# explain with x as (select * from test_table ) select * from x limit 1;
                             QUERY PLAN
---------------------------------------------------------------------
 Limit  (cost=0.00..0.02 rows=1 width=11)
   ->  Seq Scan on test_table  (cost=0.00..18.00 rows=1000 width=11)
(2 rows)

```

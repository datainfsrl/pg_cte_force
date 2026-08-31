create database testdb;
\c testdb;
create table test_table as (select generate_series(1, 1000) as id, 'test' || generate_series(1, 1000)::text as name);
create index idx_test_table_name on test_table(name);
alter database testdb set pg_cte_force.mode = 'materialized';
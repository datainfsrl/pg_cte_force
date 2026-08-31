psql -v ON_ERROR_STOP=1 \
     --username "$POSTGRES_USER" \
     --dbname "$POSTGRES_DB" \
     -c "ALTER SYSTEM SET shared_preload_libraries = 'pg_cte_force';"

pg_ctl -m fast -w restart
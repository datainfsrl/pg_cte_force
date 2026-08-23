# pg_cte_force — ambiente Docker di sviluppo

Container per compilare/testare `pg_cte_force` e per far lavorare Claude Code
direttamente dentro l'ambiente (stessa toolchain, stesso `pg_config`, stesso
server Postgres).

## Struttura

```
.
├── Dockerfile
├── docker-compose.yml
└── src/                      <-- montato in /extension nel container
    ├── pg_cte_force.c
    ├── pg_cte_force.control
    ├── pg_cte_force--1.0.sql
    ├── Makefile
    └── build.sh
```

## Avvio

```bash
export ANTHROPIC_API_KEY=sk-ant-...      # necessario solo se vuoi usare `claude` dentro il container
docker compose build
docker compose up -d
```

Il server Postgres (default: versione 18, vedi `PG_MAJOR` in
`docker-compose.yml`) parte subito con l'utente `postgres` / password
`devpass`, database `extdev`, esposto su `localhost:5433`.

## Compilare l'estensione

```bash
docker compose exec pg-dev bash
cd /extension
./build.sh
```

`make install` scrive il `.so` nel `pkglibdir` della stessa istanza di
Postgres in esecuzione nel container, quindi non serve copiare nulla altrove.

## Due modalità di test

### 1. Caricamento manuale per sessione (iterazione rapida, no restart)

```sql
LOAD 'pg_cte_force';
SET pg_cte_force.mode = 'materialized';
EXPLAIN (COSTS OFF)
WITH x AS (SELECT * FROM tbl WHERE ...) SELECT * FROM x JOIN ...;
```

Comodo mentre modifichi il codice: ricompili, riapri la sessione psql, `LOAD`
di nuovo. Nessun riavvio del server.

### 2. shared_preload_libraries (comportamento "di produzione")

Questo è il caso reale che avevi in mente — `ALTER DATABASE ... SET` che si
applica automaticamente a ogni nuova sessione, senza `LOAD` esplicito:

```sql
ALTER SYSTEM SET shared_preload_libraries = 'pg_cte_force';
```

poi, da fuori il container:

```bash
docker compose restart pg-dev
```

A quel punto:

```sql
ALTER DATABASE extdev SET pg_cte_force.mode = 'not_materialized';
```

si applica a ogni nuova connessione su `extdev`, senza bisogno di `LOAD`.

⚠️ Attenzione all'ordine: se metti `shared_preload_libraries` in
`postgresql.conf` (o via `ALTER SYSTEM`) **prima** di aver mai fatto
`make install`, il riavvio di Postgres fallisce perché il `.so` non esiste
ancora. Builda sempre l'estensione almeno una volta prima di abilitare il
preload.

## Usare Claude Code nel container

```bash
docker compose exec pg-dev bash
claude
```

Il container gira come `root` (necessario per l'entrypoint originale
dell'immagine `postgres`, che poi fa `gosu postgres` per il processo del
server). Se Claude Code si rifiuta di partire come root, usa:

```bash
claude --dangerously-skip-permissions
```

Se preferisci isolare l'agente da root, valuta di creare un utente `dev`
dedicato nel Dockerfile e lanciare `docker compose exec -u dev pg-dev bash`
per le sessioni interattive (il server Postgres resta gestito dall'utente
`postgres` come da immagine originale).

## Testare su un'altra major version

```bash
docker compose build --build-arg PG_MAJOR=14
docker compose up -d
```

Nota: essendo un volume Docker (`pgdata`) diverso per data directory
incompatibili tra major version, se cambi versione conviene ripartire da un
volume pulito:

```bash
docker compose down -v
docker compose build --build-arg PG_MAJOR=14
docker compose up -d
```

## Nota sulla struttura dati (Postgres 18+)

Dalla versione 18 le immagini ufficiali `postgres` sono cambiate: i dati
vanno montati su `/var/lib/postgresql` (non più su
`/var/lib/postgresql/data`), perché ora usano una struttura di directory
compatibile con `pg_ctlcluster` (sottodirectory per major version, per
supportare `pg_upgrade --link` senza problemi di mount point). Il
`docker-compose.yml` in questo repo è già impostato correttamente per PG18+.

Se stai passando da una versione precedente di questo setup (o da un volume
creato con un'immagine Postgres <18), l'errore tipico è:

```
Error: in 18+, these Docker images are configured to store database data in a
       format which is compatible with "pg_ctlcluster" ...
       Counter to that, there appears to be PostgreSQL data in:
         /var/lib/postgresql/data (unused mount/volume)
```

Soluzione: il volume vecchio non è riusabile nel nuovo formato, va ricreato:

```bash
docker compose down -v
docker compose up -d
```

Se invece devi testare esplicitamente una versione < 18 (es. `PG_MAJOR=14`),
il mount tradizionale su `/var/lib/postgresql/data` va bene per quelle
immagini — ma dato che questo compose è pensato per PG18 di default, se
scendi di versione ricordati di verificare il path corretto per l'immagine
specifica che stai usando.

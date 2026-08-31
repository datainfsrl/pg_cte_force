#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
make clean
make
make install
echo "OK: pg_cte_force.so installato in $(pg_config --pkglibdir)"
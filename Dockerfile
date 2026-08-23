ARG PG_MAJOR=19
FROM postgres:${PG_MAJOR}-bookworm

ARG PG_MAJOR=19
ENV DEBIAN_FRONTEND=noninteractive \
    PG_MAJOR=${PG_MAJOR}

# --- Toolchain di build per l'estensione ---------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        postgresql-server-dev-${PG_MAJOR} \
        git \
        curl \
        ca-certificates \
        gnupg \
        less \
        vim \
        gdb \
        sudo \
        locales \
    && rm -rf /var/lib/apt/lists/*

# --- Node.js 22.x (richiesto da @anthropic-ai/claude-code >=22.0.0) ------
RUN curl -fsSL https://deb.nodesource.com/setup_22.x | bash - \
    && apt-get install -y --no-install-recommends nodejs \
    && rm -rf /var/lib/apt/lists/*

RUN npm install -g @anthropic-ai/claude-code

# --- Working dir dell'estensione (montato via bind mount) ----------------
WORKDIR /extension

# Nota: NON impostiamo USER qui. L'entrypoint originale dell'immagine
# postgres deve girare come root per poi fare gosu verso l'utente
# "postgres" per il processo del server. Le sessioni interattive
# (docker compose exec) partiranno quindi come root: comodo per compilare
# l'estensione e lanciare Claude Code, ma ricordati di usare
# `claude --dangerously-skip-permissions` se lanci l'agente da root,
# oppure crea un utente non privilegiato se preferisci maggiore isolamento.

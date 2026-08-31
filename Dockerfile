ARG PG_VERSION=19beta3
ARG PG_MAJOR=19

FROM postgres:${PG_VERSION}-bookworm

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

# --- Node.js 22.x ---------------------------------------------------------
RUN curl -fsSL https://deb.nodesource.com/setup_22.x | bash - \
    && apt-get install -y --no-install-recommends nodejs \
    && rm -rf /var/lib/apt/lists/*

RUN npm install -g @anthropic-ai/claude-code

WORKDIR /extension

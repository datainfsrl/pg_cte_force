ARG PG_VERSION=19beta3
ARG PG_MAJOR=19

FROM postgres:${PG_VERSION}-bookworm

ARG PG_MAJOR=19

ENV DEBIAN_FRONTEND=noninteractive \
    PG_MAJOR=${PG_MAJOR}

# --- Toolchain di build for the extension ---------------------------------
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
    nano \
    && rm -rf /var/lib/apt/lists/*

# First compile the extension, then install it into the PostgreSQL server's library directory
WORKDIR /extension

COPY ./src/ /extension/

RUN make clean \
    && make \
    && make install
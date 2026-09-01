FROM debian:bookworm-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        g++ \
        libssl-dev \
        libtbb-dev \
        make \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY Makefile *.cpp *.hpp *.h ./
RUN make -j2 OPT='-O3 -DNOLOG' jugglucoconnect

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libssl3 \
        libtbb12 \
        zlib1g \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/jugglucoconnect /usr/local/bin/jugglucoconnect

WORKDIR /data
USER 65534:65534
EXPOSE 6789/tcp

ENTRYPOINT ["/usr/local/bin/jugglucoconnect"]
CMD ["6789"]

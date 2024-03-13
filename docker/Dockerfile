FROM debian:bullseye-slim AS open-ocpp-simu-compile

SHELL ["/bin/bash", "-c"]

ARG user=default
ARG group=default
ARG uid=1000
ARG gid=1000

# -y by default for apt-get
RUN echo "APT::Get::Assume-Yes "true";" >> /etc/apt/apt.conf.d/aptconf


RUN apt-get update && apt-get install build-essential clang cmake python3 python3-pip python3-venv wget libpaho-mqtt-dev git openssl libssl-dev sqlite3 libsqlite3-dev zip curl tcpdump libcap2-bin\
&&  apt-get -q autoremove  &&  rm -rf "/var/lib/apt/lists/*"

RUN git clone https://github.com/c-jimenez/open-ocpp.git

RUN cd open-ocpp && git fetch --tags && git checkout v1.5.1 && make gcc && make install-gcc

RUN apt-get update && apt-get install pkg-config &&  apt-get -q autoremove  &&  rm -rf "/var/lib/apt/lists/*"

# Ensure user exist within container with same uid/gid
RUN groupadd -g ${gid} ${group} && useradd -m -u ${uid} -g ${gid} ${user}

USER ${uid}:${gid}
ENTRYPOINT ["/bin/bash"]
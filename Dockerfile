FROM gcc:latest

RUN apt-get update && apt-get -y --no-install-recommends install \
    build-essential \
    gdb \
    wget \
    make \
    libboost-dev \
    vim

RUN ln -s /usr/bin/python3 /usr/bin/python

RUN mkdir /usr/src/mmorpg

COPY . /usr/src/mmorpg

WORKDIR /usr/src/mmorpg

CMD ["bash", "-c", "while true; do echo Hello Docker; sleep 1; done"]
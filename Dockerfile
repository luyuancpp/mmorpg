FROM gcc:latest

RUN mkdir /usr/src/turn-based-game

COPY . /usr/src/turn-based-game

COPY ./tools/protoc /usr/bin

WORKDIR /usr/src/turn-based-game

RUN ln -s /usr/bin/python3 /usr/bin/python

RUN apt-get update && apt-get -y --no-install-recommends install \
    build-essential \
    clang \
    cmake \
    gdb \
    wget

CMD ["bash", "-c", "while true; do echo Hello Docker; sleep 1; done"]
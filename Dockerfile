FROM gcc:latest

RUN apt-get update && apt-get -y --no-install-recommends install \
    build-essential \
    cmake \
    gdb \
    wget \ 
    make  \
    libboost-dev 

RUN ln -s /usr/bin/python3 /usr/bin/python

RUN mkdir /usr/src/turn-based-game

COPY . /usr/src/turn-based-game

WORKDIR /usr/src/turn-based-game


CMD ["bash", "-c", "while true; do echo Hello Docker; sleep 1; done"]
#CMD ["./autogen.sh"]
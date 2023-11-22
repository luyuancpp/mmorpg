FROM gcc:latest

RUN apt-get update && apt-get -y --no-install-recommends install \
    build-essential \
    cmake \
    gdb \
    wget \ 
    make \
    libboost-dev \
    golang 

RUN ln -s /usr/bin/python3 /usr/bin/python

RUN mkdir /usr/src/turn-based-game

COPY . /usr/src/turn-based-game

WORKDIR /usr/src/turn-based-game/third_party/abseil-cpp

RUN cmake -DABSL_BUILD_TESTING=ON -DABSL_USE_GOOGLETEST_HEAD=ON -DCMAKE_CXX_STANDARD=20 .

CMD ["./autogen.sh"]
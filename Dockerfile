FROM gcc:latest

RUN mkdir /usr/src/turn-based-game

COPY . /usr/src/turn-based-game

WORKDIR /usr/src/turn-based-game

RUN ln -s /usr/bin/python3 /usr/bin/python

RUN apt-get update && apt-get -y --no-install-recommends install \
    build-essential \
    cmake \
    gdb \
    wget \ 
    make \
    libboost-dev \
    vim \
    golang \
    brew

RUN brew install bazel

RUN cd third_party/protobuf/

RUN bazel build :protoc :protobuf

RUN cp bazel-bin/protoc /usr/bin

CMD ["./autogen.sh"]
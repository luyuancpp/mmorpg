FROM gcc:latest

RUN mkdir /usr/src/turn-based-game

COPY . /usr/src/turn-based-game


WORKDIR /usr/src/turn-based-game

RUN ln -s /usr/bin/python3 /usr/bin/python

CMD ["bash", "-c", "while true; do echo Hello Docker; sleep 1; done"]
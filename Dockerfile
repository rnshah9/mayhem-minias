FROM --platform=linux/amd64 ubuntu:20.04

RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential peg

ADD . /minias
WORKDIR /minias
RUN make

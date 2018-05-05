# This is a comment
FROM ubuntu:18.04
MAINTAINER me <little.mole@oha7.org>

# std dependencies
RUN DEBIAN_FRONTEND=noninteractive apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get upgrade -y
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential g++ \
libgtest-dev cmake git pkg-config valgrind sudo joe wget \
openssl libssl-dev libevent-dev uuid-dev \
clang libc++-dev libc++abi-dev \
libboost-dev libboost-system-dev libhiredis-dev redis-server \
mysql-server libmysqlclient-dev

ARG CXX=g++
ENV CXX=${CXX}

# compile gtest with given compiler
ADD ./docker/gtest.sh /usr/local/bin/gtest.sh
RUN /usr/local/bin/gtest.sh

ARG BACKEND=libevent
ENV BACKEND=${BACKEND}

ARG BUILDCHAIN=make
ENV BUILDCHAIN=${BUILDCHAIN}

# build dependencies
ADD ./docker/build.sh /usr/local/bin/build.sh
ADD ./docker/install.sh /usr/local/bin/install.sh
ADD ./docker/mysql.sh /usr/local/bin/mysql.sh

RUN /usr/local/bin/install.sh cryptoneat 
RUN /usr/local/bin/install.sh repro 
RUN /usr/local/bin/install.sh prio 

RUN mkdir -p /usr/local/src/repro-mysql
ADD . /usr/local/src/repro-mysql

RUN /usr/local/bin/mysql.sh

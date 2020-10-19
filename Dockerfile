# This is a comment
FROM littlemole/devenv_clangpp_make
MAINTAINER me <little.mole@oha7.org>

# std dependencies
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y mysql-server libmysqlclient-dev

ARG CXX=g++
ENV CXX=${CXX}

ARG BACKEND=libevent
ENV BACKEND=${BACKEND}

ARG BUILDCHAIN=make
ENV BUILDCHAIN=${BUILDCHAIN}

ARG TS=
ENV TS=${TS}

# build dependencies
ADD ./docker/mysql.sh /usr/local/bin/mysql.sh

RUN /usr/local/bin/install.sh repro 
RUN /usr/local/bin/install.sh prio 
RUN /usr/local/bin/install.sh repro-curl 

RUN mkdir -p /usr/local/src/repro-mysql
ADD . /usr/local/src/repro-mysql

RUN /usr/local/bin/mysql.sh

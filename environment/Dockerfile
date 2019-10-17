FROM ubuntu:latest

# install necessary packages for building pintos
RUN \
  apt-get update && \
  apt-get -y install \
    build-essential \
    git \
    time \
    gdb \
    qemu-utils \
    qemu-system-x86 \
    && \
  rm -rf /var/lib/apt/lists/*

# install editors
RUN \
  apt-get update && \
  apt-get -y install \
    vim \
    emacs \
    nano \
    && \
  rm -rf /var/lib/apt/lists/*

# create a user so we're not running as root
RUN \
  groupadd -g 1000 pintos && \
  useradd -u 1000 -m -g pintos -p pintos -s /bin/bash pintos
USER pintos:pintos
WORKDIR /home/pintos

# set pintos env variables in bashrc
RUN \
  echo PINTOSPATH=/home/pintos/pintos/src >> .bashrc && \
  echo 'PATH=$PINTOSPATH/utils:$PATH' >> .bashrc

FROM debian:bookworm
RUN apt-get update
RUN apt-get -y upgrade
RUN apt-get -y install gcc make perl-modules
RUN apt-get -y install flex bison wget libssl-dev libelf-dev bc
ENTRYPOINT [ "/src/ci/build-dahdi.sh" ]

#!/bin/sh
set -e

if [ "$1" != "" ]; then
  JOBS=$1
fi

if command -v apt >/dev/null; then
  if [ ! -e /root/last-update ]; then
    date > /root/last-update
  fi
  if [ -z "$(find /root/last-update -mmin -60)" ]; then
    apt-get update && apt-get install -y linux-headers-amd64
    date > /root/last-update
  fi
  KERNELS=$(find /usr/src -maxdepth 1 -type d -name 'linux-headers-*' -not -name '*common*')
else
  if command -v dnf > /dev/null ; then
    dnf update -y dnf kernel-devel
  else
    yum update -y kernel-devel
  fi
  KERNELS=$(find /usr/src/kernels -maxdepth 1 -type d -regextype sed -regex '.*[.]\(el\|fc\).*')
fi

if [ "$KERNELS" = "" ]; then
  echo >&2 "Failed to find any kernels"
  exit 1
fi

# Copy the source into the container so we can run builds in parallel
rm -fr /root/code
cp -fr /source /root/code
cd /root/code

for KSRC in $KERNELS ; do
  echo "Building against $KSRC"
  export KSRC=$KSRC
  make -s clean
  make -s -j $(grep -c "^processor" /proc/cpuinfo)
done

#!/bin/sh
rm -rf /src/build_logs
mkdir -p /src/build_logs
cd /linux
echo "### Cleaning Kernel tree"
echo "  # rm .config"
rm .config

echo "  # make clean"
make clean
echo "### Get kernel version"
make kernelversion > /src/build_logs/kernel_version.txt
echo "### Create defconfig and prepare for module building"
echo "  # make x86_64_defconfig"
make x86_64_defconfig
echo "  # make modules_prepare"
make modules_prepare

echo " # make kernel"
make

echo " #make modules"
make modules

echo "### Building DAHDI modules and installing"
cd /src
echo "  # make clean"
make clean
echo "  # make install KSRC=/linux"
make install KSRC=/linux 2>&1 | tee /src/build_logs/build_log.txt


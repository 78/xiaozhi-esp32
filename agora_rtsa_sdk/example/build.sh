#!/bin/bash

# Build SDK demo examples
#
# Usage: build.sh [options]
#
# Options:
#   -a <arch>       target architecture                   (default: x86_64)
#   -f <toolchain>  cmake toolchain file                  (default: none)
#   -t <type>       build type: release|debug|asan        (default: release)
#   -b <op>         build operation: build|clean|rebuild  (default: build)
#   -h              show help

CURRENT=$(cd "$(dirname "$0")" && pwd)

arch="x86_64"
build_type="release"
toolchain=""
build_op="build"

function cmake_build() {
    rm -rf build
    mkdir build && cd build \
    && cmake $CURRENT \
        -DMACHINE=$arch \
        -DCMAKE_TOOLCHAIN_FILE=$toolchain \
        -DCMAKE_BUILD_TYPE=$build_type \
        -Wno-dev \
    && make -j8
}

function clean_build_files() {
    build_files=("build")
    for file in ${build_files[@]}; do
        if [ -d ${file} ]; then
            rm -rf ${file}
        elif [ -f ${file} ]; then
            rm ${file}
        fi
    done
}

function help() {
    echo -e "$0 [-b <build_op>] [-l <license>] [-t <type>] [-f <toolchain.cmake>]"
    echo -e "\t -a <arch>,       target architecture, default: x86_64"
    echo -e "\t -f <toolchain>,  cmake toolchain file, default: none"
    echo -e "\t -t <type>, release|debug|asan, default: release"
    echo -e "\t -b <build_op>, build|clean|rebuild, default: build"
}

while getopts 'a:f:t:b:h' opt; do
    case $opt in
    a)
        arch=${OPTARG}
        ;;
    f)
        toolchain=${OPTARG}
        ;;
    t)
        build_type=${OPTARG}
        ;;
    b)
        build_op=${OPTARG}
        ;;
    h)
        help
        exit 1
        ;;
    \?)
        help
        exit 1
        ;;
    esac
done

if [ "${build_op}" = "build" ]; then
    echo "building ..."
    cmake_build $CURRENT || exit 1
    echo "build done"
elif [ "${build_op}" = "clean" ]; then
    echo "cleaning ..."
    clean_build_files || exit 1
    echo "clean done"
elif [ "${build_op}" = "rebuild" ]; then
    echo "rebuilding ..."
    clean_build_files || exit 1
    cmake_build $CURRENT || exit 1
    echo "rebuild done"
else
    echo "unknown build_op: "${build_op}""
    exit 1
fi

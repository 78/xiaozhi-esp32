#!/bin/bash

# Build third-party libraries for the target architecture.
#
# Usage: build.sh [options]
#
# Options:
#   -a <arch>            target architecture, x86_64|aarch64|..(default: x86_64)
#   -b <op>              build operation: build|clean|rebuild  (default: build)
#   -t <type>            build type: release|debug|asan        (default: release)
#   -f <toolchain.cmake> CMake toolchain file for cross-compilation (default: none)
#   -h                   show help

CURRENT=$(cd "$(dirname "$0")" && pwd)

arch="x86_64"
build_op="build"
build_type="release"
toolchain=""

function cmake_build() {
    src=$(cd "$1" && pwd)
    shift
    if [ -d build ]; then
        rm -rf build
    fi

    if [ ${build_type} == "debug" ]; then
        debug_flag="-DCMAKE_BUILD_TYPE=Debug "
    elif [ ${build_type} = "asan" ]; then
        debug_flag="-DCMAKE_BUILD_TYPE=asan "
    elif [ ${build_type} = "release" ]; then
        debug_flag="-DCMAKE_BUILD_TYPE=release "
    fi

    if [ -n "$toolchain" ]; then
        echo "using toolchain: $toolchain"
        os_flag="-DCMAKE_TOOLCHAIN_FILE=$toolchain"
    fi

    mkdir build && cd build &&
	cmake $src $debug_flag $os_flag $* &&
        make -j8 || return 1
    cd -
}

function clean_build_files() {
    rm -rf build
    find $CURRENT -name "*.a" | xargs rm -rf
}

function help() {
    echo -e "$0 [-a <arch>] [-b <build_op>] [-t <type>] [-f <toolchain.cmake>]"
    echo -e "\t -a <arch>,     target architecture, default: x86_64"
    echo -e "\t -b <build_op>, build|clean|rebuild, default: build"
    echo -e "\t -t <type>, release|debug|asan, default: release"
    echo -e "\t -f <toolchain.cmake>, default: NONE"
}

while getopts 'a:b:t:f:h' opt; do
    case $opt in
    a)
        arch=${OPTARG}
        ;;
    b)
        build_op=${OPTARG}
        ;;
    t)
        build_type=${OPTARG}
        ;;
    f)
        toolchain=${OPTARG}
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

if [ "${build_op}" != "build" -a "${build_op}" != "rebuild" -a "${build_op}" != "clean" ]; then
    echo "error build_op: "${build_op}""
    exit 1
fi

if [ "${build_op}" = "build" ]; then
    echo "building ..."
    for d in $(ls $CURRENT)
    do
	    test -f $CURRENT/$d/CMakeLists.txt && ( cmake_build $CURRENT/$d -DMACHINE=${arch} || exit 1 )
    done
    echo "build done"
elif [ "${build_op}" = "clean" ]; then
    echo "cleaning ..."
    clean_build_files || exit 1
    echo "clean done"
elif [ "${build_op}" = "rebuild" ]; then
    echo "rebuilding ..."
    clean_build_files || exit 1
    for d in $(ls $CURRENT)
    do
	    test -f $CURRENT/$d/CMakeLists.txt && ( cmake_build $CURRENT/$d -DMACHINE=${arch} || exit 1 )
    done
    echo "rebuild done"
fi

#!/usr/bin/python3

"""
This script tries to build a project with CMake, Meson, and Autotools.
It checks if CMake, Meson, and Autotools are installed, and performs
the configure, build, and optionally test steps for each build system.
"""

import multiprocessing
import os
import random
import string
import subprocess
import sys
import shutil


if sys.platform == "win32":
    REPO_DIR = "\\".join(os.path.abspath(__file__).split("\\")[:-2])
else:
    REPO_DIR = "/".join(os.path.abspath(__file__).split("/")[:-2])


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--test", action="store_true", help="Run tests")
    parser.add_argument("--cmake", action="store_true", help="Only run CMake")
    parser.add_argument("--meson", action="store_true", help="Only run Meson")
    if sys.platform != "win32":
        parser.add_argument("--autotools", action="store_true", help="Only run Autotools")
    args = parser.parse_args()

    test = args.test
    cmake_only = args.cmake
    meson_only = args.meson
    if sys.platform != "win32":
        autotools_only = args.autotools
    autotools_only = False

    result = []
    os.chdir(REPO_DIR)

    # download model
    if sys.platform == "win32":
        run_command("autogen.bat")
    else:
        run_command("./autogen.sh")

    if sys.platform != "win32" and not cmake_only and not meson_only:
        result += autotools_build(test)

    if not autotools_only and not meson_only:
        result += cmake_build(test)
        result += cmake_build(test, extra_options="-DOPUS_NEURAL_FEC=ON")

    if not autotools_only and not cmake_only:
        result += meson_build(test)

    print_result(result, test)

def print_result(result, test=False):
    if len(result) == 0:
        print("No results available")
        return

    headers = ["Name", "Build Passed"]
    if test:
        headers.append("Test Passed")

    # Calculate the maximum width for each column
    column_widths = [max(len(str(row[i])) for row in result) for i in range(len(headers))]

    # Print the headers
    header_row = " | ".join(f"{header: <{column_widths[i]}}" for i, header in enumerate(headers))
    print(header_row)
    print("-" * len(header_row))

    # Print the data rows
    for row in result:
        row_values = [str(value) for value in row[:len(headers)]]  # Include values up to the last column to be printed
        row_string = " | ".join(f"{row_values[i]: <{column_widths[i]}}" for i in range(len(headers)))
        print(row_string)

def autotools_build(test=False):
    build = "Autotools"
    autotools_build_succeeded = False
    autotools_test_succeeded = False

    if not check_tool_installed("autoreconf") or not check_tool_installed("automake") or not check_tool_installed("autoconf"):
        print("Autotools dependencies are not installed. Aborting.")
        print("Install with: sudo apt-get install git autoconf automake libtool gcc make")
        return [(build, autotools_build_succeeded, autotools_test_succeeded)]

    run_command("./configure")
    if run_command("make -j {}".format(get_cpu_core_count())) == 0:
        autotools_build_succeeded = True
    if test:
        if run_command("make check -j {}".format(get_cpu_core_count())) == 0:
            autotools_test_succeeded = True

    return [(build, autotools_build_succeeded, autotools_test_succeeded)]


def cmake_build(test=False, extra_options=""):
    cmake_build_succeeded = False
    cmake_test_succeeded = False
    build = "CMake"

    if not check_tool_installed("cmake"):
        print("CMake is not installed. Aborting.")
        if sys.platform != "win32":
            print("Install with: sudo apt install cmake")
        else:
            print("Download and install from: https://cmake.org/download/")
        return [(build, cmake_build_succeeded, cmake_test_succeeded)]

    if not check_tool_installed("ninja"):
        print("Ninja is not installed. Aborting.")
        if sys.platform != "win32":
            print("Install with: sudo apt ninja-build ")
        else:
            print("Download and install from: https://ninja-build.org/")
        return [(build, cmake_build_succeeded, cmake_test_succeeded)]

    cmake_build_dir = create_dir_with_random_postfix("cmake-build")
    cmake_cfg_cmd = ["cmake", "-S" ".", "-B", cmake_build_dir, "-G", '"Ninja"', "-DCMAKE_BUILD_TYPE=Release", "-DOPUS_BUILD_TESTING=ON", "-DOPUS_BUILD_PROGRAMS=ON", "-DOPUS_FAST_MATH=ON", "-DOPUS_FLOAT_APPROX=ON"]
    cmake_cfg_cmd += [option for option in extra_options.split(" ")]
    run_command(" ".join(cmake_cfg_cmd))
    cmake_build_cmd = ["cmake", "--build", cmake_build_dir, "-j", "{}".format(get_cpu_core_count())]
    cmake_test_cmd = ["ctest", "-j", "{}".format(get_cpu_core_count())]
    if sys.platform == "win32":
        cmake_build_cmd += ["--config", "Release"]
        cmake_test_cmd += ["-C", "Release"]

    if run_command(" ".join(cmake_build_cmd)) == 0:
        cmake_build_succeeded = True

    if test:
        os.chdir(cmake_build_dir)
        if run_command(" ".join(cmake_test_cmd)) == 0:
            cmake_test_succeeded = True
        os.chdir(REPO_DIR)

    shutil.rmtree(cmake_build_dir)

    if extra_options != "":
        build += " with options: "
        build += extra_options.replace("-D", "")
    return [(build, cmake_build_succeeded, cmake_test_succeeded)]

def meson_build(test=False, extra_options=""):
    build = "Meson"
    meson_build_succeeded = False
    meson_test_succeeded = False

    if not check_tool_installed("meson"):
        print("Meson is not installed. Aborting.")
        print("Install with: pip install meson")
        return [(build, meson_build_succeeded, meson_test_succeeded)]

    if not check_tool_installed("ninja"):
        print("Ninja is not installed. Aborting.")
        if sys.platform != "win32":
            print("Install with: sudo apt ninja-build ")
        else:
            print("Download and install from: https://ninja-build.org/")
        return [(build, meson_build_succeeded, meson_test_succeeded)]

    meson_build_dir = create_dir_with_random_postfix("meson-build")
    meson_cfg_cmd = ["meson", "setup", meson_build_dir]
    meson_cfg_cmd += [option for option in extra_options.split(" ")]
    run_command(" ".join(meson_cfg_cmd))
    meson_build_cmd = ["meson", "compile", "-C", meson_build_dir]
    meson_test_cmd = ["meson", "test", "-C", meson_build_dir]

    if run_command(" ".join(meson_build_cmd)) == 0:
        meson_build_succeeded = True

    if test:
        os.chdir(meson_build_dir)
        if run_command(" ".join(meson_test_cmd)) == 0:
            meson_test_succeeded = True
        os.chdir(REPO_DIR)

    shutil.rmtree(meson_build_dir)

    if extra_options != "":
        build += " with options: "
        build += extra_options.replace("-D", "")
    return [(build, meson_build_succeeded, meson_test_succeeded)]

def create_dir_with_random_postfix(dir):
    random_chars = "".join(random.choices(string.ascii_letters, k=3))
    dir = dir + "-" + random_chars
    dir = os.path.join(os.getcwd(), dir)
    os.makedirs(dir)
    return dir

def check_tool_installed(tool):
    if sys.platform == "win32":
        try:
            # Use the "where" command to search for the executable
            subprocess.check_output(["where", tool], shell=True)
            return True
        except subprocess.CalledProcessError:
            return False
    else:
        return run_command(f"command -v {tool}") == 0

def run_command(command):
    process = subprocess.Popen(command, shell=True)
    process.communicate()
    return process.returncode

def get_cpu_core_count():
    return int(max(multiprocessing.cpu_count(), 1))

if __name__ == "__main__":
    main()

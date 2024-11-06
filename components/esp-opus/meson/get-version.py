#!/usr/bin/env python3
#
# Opus get-version.py
#
# Extracts versions for build:
#  - Opus package version based on 'git describe' or $srcroot/package_version
#  - libtool version based on configure.ac
#  - macos lib version based on configure.ac
#
# Usage:
# get-version.py [--package-version | --libtool-version | --darwin-version]
import argparse
import subprocess
import os
import sys
import shutil

if __name__ == '__main__':
    arg_parser = argparse.ArgumentParser(description='Extract Opus package version or libtool version')
    group = arg_parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--libtool-version', action='store_true')
    group.add_argument('--package-version', action='store_true')
    group.add_argument('--darwin-version', action='store_true')
    args = arg_parser.parse_args()

    srcroot = os.path.normpath(os.path.join(os.path.dirname(__file__), '..'))

    # package version
    if args.package_version:
        package_version = None

        # check if git checkout
        git_dir = os.path.join(srcroot, '.git')
        is_git = os.path.isdir(git_dir) or os.path.isfile(git_dir)
        have_git = shutil.which('git') is not None

        if is_git and have_git:
            git_cmd = subprocess.run(['git', '--git-dir=' + git_dir, 'describe', 'HEAD'], stdout=subprocess.PIPE)
            if git_cmd.returncode:
                print('ERROR: Could not extract package version via `git describe` in', srcroot, file=sys.stderr)
                sys.exit(-1)
            package_version = git_cmd.stdout.decode('ascii').strip().lstrip('v')
        else:
            with open(os.path.join(srcroot, 'package_version'), 'r') as f:
                for line in f:
                    if line.startswith('PACKAGE_VERSION="'):
                        package_version = line[17:].strip().lstrip('v').rstrip('"')
                    if package_version:
                        break

        if not package_version:
            print('ERROR: Could not extract package version from package_version file in', srcroot, file=sys.stderr)
            sys.exit(-1)

        print(package_version)
        sys.exit(0)

    # libtool version + darwin version
    elif args.libtool_version or args.darwin_version:
        opus_lt_cur = None
        opus_lt_rev = None
        opus_lt_age = None

        with open(os.path.join(srcroot, 'configure.ac'), 'r') as f:
            for line in f:
                if line.strip().startswith('OPUS_LT_CURRENT='):
                    opus_lt_cur = line[16:].strip()
                elif line.strip().startswith('OPUS_LT_REVISION='):
                    opus_lt_rev = line[17:].strip()
                elif line.strip().startswith('OPUS_LT_AGE='):
                    opus_lt_age = line[12:].strip()

        if opus_lt_cur and opus_lt_rev and opus_lt_age:
            opus_lt_cur = int(opus_lt_cur)
            opus_lt_rev = int(opus_lt_rev)
            opus_lt_age = int(opus_lt_age)
            if args.libtool_version:
              print('{}.{}.{}'.format(opus_lt_cur - opus_lt_age, opus_lt_age, opus_lt_rev))
            elif args.darwin_version:
              print('{}.{}.{}'.format(opus_lt_cur + 1, 0, 0))
            sys.exit(0)
        else:
            print('ERROR: Could not extract libtool version from configure.ac file in', srcroot, file=sys.stderr)
            sys.exit(-1)
    else:
        sys.exit(-1)

#!/usr/bin/env bash
set -e
cd $(dirname $0)
echo "#define LIB_PATH $(cd .. && pwd)" > lib_path.h

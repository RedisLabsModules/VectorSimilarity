#!/bin/bash
set -e
export DEBIAN_FRONTEND=noninteractive
MODE=$1 # whether to install using sudo or not

$MODE apt-get update -qq || true
$MODE apt-get install -yqq gcc-12 g++-12 git wget build-essential valgrind lcov
source install_cmake.sh $MODE

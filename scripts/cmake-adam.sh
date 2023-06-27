#!/bin/bash

export BOOST_HOME=/opt/homebrew/Cellar/boost/1.82.0_1/include

UDA_HOME=/Users/aparker/UDADevelopment/install
export PKG_CONFIG_PATH=$UDA_HOME/lib/pkgconfig:$PKG_CONFIG_PATH

cmake -Bbuild_adam -H. -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=$UDA_HOME \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    "$@"

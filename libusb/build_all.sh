#!/bin/bash

mkdir tmp;

make clean
make DDK_ROOT_PATH=c:/WINDDK BUILD_BCC_LIB=1 BUILD_MSVC_LIB=1 dist



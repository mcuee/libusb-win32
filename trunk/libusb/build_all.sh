#!/bin/bash

echo 
mkdir tmp;

make clean
make DDK_PATH=c:/WINDDK DDK_BUILD_CHECKED=1 BUILD_BCC_LIB=1 BUILD_MSVC_LIB=1 \
    bin_dist
mv *.tgz ./tmp

make clean
make DDK_PATH=c:/WINDDK DDK_BUILD_CHECKED=0 BUILD_BCC_LIB=1 BUILD_MSVC_LIB=1 \
    bin_dist
mv *.tgz ./tmp

make clean
make src_dist
mv ./tmp/*.tgz .
rm -rf ./tmp



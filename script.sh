#!/bin/bash

file=build

if [ -d $file ];then
    rm -rf $file
fi

mkdir $file && cd $file

cmake ..

make

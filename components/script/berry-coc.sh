#!/bin/bash

rm generate/*

python3 lib/berry/tools/coc/coc -o generate lib/berry/src src -c src/berry_conf.h

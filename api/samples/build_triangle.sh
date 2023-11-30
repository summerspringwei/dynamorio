#!/bin/bash
set -xe
as -g -o triangle.o triangle.s
ld triangle.o -o triangle.out
./triangle.out

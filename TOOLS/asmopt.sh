#!/bin/sh
# Write and test your MMX-SSE optimizations!!!

#make asmopt
#./asmopt
test -f asmopt.mmx2	&& diff --brief asmopt.gen asmopt.mmx2
test -f asmopt.mmx	&& diff --brief asmopt.gen asmopt.mmx
test -f asmopt.sse2	&& diff --brief asmopt.gen asmopt.sse2
test -f asmopt.sse3	&& diff --brief asmopt.gen asmopt.sse3

#!/bin/sh
sync
sleep 2
./fastmem-k6
sleep 2
./fastmem-k7
sleep 2
./fastmem-mmx
sleep 2
./fastmem-sse
sleep 2
./fastmem-k7xp

#!/bin/bash

echo "Test #1 : testing transfer of file.txt -> received.txt"
../receiver :: 1025 -f ../received.txt 2> stderr.txt &
../sender ::1 1025 -f ../file.txt 2> stderr.txt

echo "Test #1 over"
cmp ../file.txt ../received.txt || echo "Test #1 failed"
sleep 2

echo "Test #2 : testing transfer of file.txt -> received.txt with bad network (1 second delay)"
../link_sim -p 1025 -P 1026 -d 1000 & ../receiver :: 1026 -f ../received.txt 2> stderr.txt &
../sender ::1 1025 -f ../file.txt 2> stderr.txt

echo "Test #2 over"
cmp ../file.txt ../received.txt || echo "Test #2 failed"
sleep 2

#!/bin/bash

echo "Cleanup"
rm -f received.txt stderr.txt link.log received.gif

echo "Making executables"
make -C ..
make -C ../LINGI1341-linksim
cp ../LINGI1341-linksim/link_sim ../link_sim

echo "Test #1 : transfer of file.txt -> received.txt with perfect network"
../receiver :: 1025 -f received.txt 2> stderr.txt &
../sender ::1 1025 -f file.txt 2> stderr.txt

echo "Test #1 over"
cmp file.txt received.txt || echo "Test #1 failed"
sleep 2

echo "Test #2 : transfer of file.txt -> received.txt with bad network (1 second delay)"
../link_sim -p 1025 -P 1026 -d 1000 &> link.log &
link_pid=$!
../receiver :: 1026 -f received.txt 2> stderr.txt &
../sender ::1 1025 -f file.txt 2> stderr.txt

kill -9 $link_pid &> /dev/null
echo "Test #2 over"
cmp file.txt received.txt || echo "Test #2 failed"
sleep 2

echo "Test #3 : transfer of file.txt -> received.txt with bad network (loss rate 5%)"
../link_sim -p 1025 -P 1026 -l 5 &> link.log &
link_pid=$!
../receiver :: 1026 -f received.txt 2> stderr.txt &
../sender ::1 1025 -f file.txt 2> stderr.txt

kill -9 $link_pid &> /dev/null
echo "Test #3 over"
cmp file.txt received.txt || echo "Test #3 failed"
sleep 2

echo "Test #4 : transfer of file.txt -> received.txt with bad network (error rate 5%)"
../link_sim -p 1025 -P 1026 -e 5 &> link.log &
link_pid=$!
../receiver :: 1026 -f received.txt 2> stderr.txt &
../sender ::1 1025 -f file.txt 2> stderr.txt

kill -9 $link_pid &> /dev/null
echo "Test #4 over"
cmp file.txt received.txt || echo "Test #4 failed"
sleep 2

echo "Test #5 : transfer of file.txt -> received.txt with bad network (jitter 2 seconds)"
../link_sim -p 1025 -P 1026 -j 2000 &> link.log &
link_pid=$!
../receiver :: 1026 -f received.txt 2> stderr.txt &
../sender ::1 1025 -f file.txt 2> stderr.txt

kill -9 $link_pid &> /dev/null
echo "Test #5 over"
cmp file.txt received.txt || echo "Test #5 failed"
sleep 2

echo "Test #6 : transfer of file.txt -> received.txt with bad netword (jitter 2 seconds, loss rate 5% and error rate 5%)"
../link_sim -p 1025 -P 1026 -j 2000 -l 5 -e 5 &> link.log &
link_pid=$!
../receiver :: 1026 -f received.txt 2> stderr.txt &
../sender ::1 1025 -f file.txt 2> stderr.txt

kill -9 $link_pid &> /dev/null
echo "Test #6 over"
cmp file.txt received.txt || echo "Test #6 failed"
sleep 2

echo "Test #7 : transfer of file.txt -> received.txt with bad network in both directions (jitter 2 seconds, loss rate 5% and error rate 5%)"
../link_sim -p 1025 -P 1026 -R -j 2000 -l 5 -e 5 &> link.log &
link_pid=$!
../receiver :: 1026 -f received.txt 2> stderr.txt &
../sender ::1 1025 -f file.txt 2> stderr.txt

kill -9 $link_pid &> /dev/null
echo "Test #7 over"
cmp file.txt received.txt || echo "Test #7 failed"
sleep 2

echo "Test #8 : transfer of bigImage.gif -> received.gif with bad network in both directions (jitter 0.5 second,  loss rate 1% and error rate 1%)"
../link_sim -p 1025 -P 1026 -R -j 500 -l 1 -e 1 &> link.log &
link_pid=$!
../receiver :: 1026 -f received.gif 2> stderr.txt &
../sender ::1 1025 -f bigImage.txt 2> stderr.txt

kill -9 $link_pid &> /dev/null
echo "Test #8 over"
cmp bigImage.gif received.gif || echo "Test #8 failed"


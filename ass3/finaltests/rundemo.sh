#!/bin/sh

echo
echo "9, 18 =======Sorting at character level, also shows data persistance=========="

./writeless
./writeless
./readless
./readless
echo

echo "10. ======Read asks for more data than in SORT=========="
./readmore
echo

echo "11.a ======Read called on empty SORT with O_NONBLOCK====="
./readmore_no_block
echo



echo "11.b =====Read blocks when called without O_NONBLOCK======"
echo ">>>>>Running read"
echo ">>>>>Write to buffer, writeless, in other session to continue the demo"
./readless
echo

echo "12, 15 =====Write blocks until complete String is written, returns when more space is made by Read====="
echo ">>>>>Run readmore manually from another session to unblock"
./writemore
echo

echo "13. ======Writing to a full SORT with NONBLOCK, should return========"
./writemore_no_block
echo

echo "14. =====Reading consumes data and frees up space========"
echo ">>>>> Ensuring that buffer is full : call write with NONBLOCK till full"
./writemore_no_block
echo ">>>>> Read off some data : call read more to read 100 chars"
./readmore
echo ">>>>> Should able to write more data : call writeless twice, 20 chars written"
./writeless
./writeless
echo

echo "16. =====IOCRESET resets the device and returns with 0 on success====="
echo ">>>>> Ensuring that buffer is full : call write with NONBLOCK till full"
./writemore_no_block
echo ">>>>>Empty the buffer using IOCRESET, returns with 0 on success"
./ioctltest
echo ">>>>>Subsequent read should hang due to empty buffer"
echo ">>Run writeless from another session to continue"
./readless
echo


echo "17. ======Tests for Concurrency=========="
echo
echo "17.a xxxxxxx Reader and Writer xxxxxxxxx"
echo ">>>>>Call Writemore in the background"
./writemore &
echo ">>>>>Call readless multiple times to read the data in chunks"
./readless
./readless
./readless
./readless
./readless
./readless
./readless
./readless
./readless
./readless
echo
echo

echo "17.b xxxxxxx Multiple Writers xxxxxxxxx"
echo ">>>>>Call Writemore and Writelss in the background"
./writemore &
./writeless &
echo ">>>>>Call readless multiple times to read the data in chunks"
./readless
./readless
./readless
./readless
./readless
./readless
./readless
./readless
./readless
./readless
echo


echo "17.c xxxxxxx Multiple Readers xxxxxxxxx"
echo ">>>>>Empty the buffer"
./ioctltest
echo ">>>>>Call readless twice in the background"
./readless &
./readless &
echo ">>>>>Call writeless to write data in the buffer"
./writeless
echo
echo

echo "19. ====Old devices preserved, Read / Write to scull and pipe========"
./sculltest
echo

echo "20. ====Invalid IOCTL commands dont run, should not return 0========="
./ioctltestfail
echo



echo "+++++++DEMO COMPLETE+++++++++"

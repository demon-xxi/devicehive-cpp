#!/bin/sh
# waits for internet connection

PING_ADDR=8.8.8.8

ping -c 1 $PING_ADDR >/dev/null 2>&1
GOT_IT=$?
while [ $GOT_IT -ne 0 ]
do
	sleep 2
	# echo -n '.'
	ping -c 1 $PING_ADDR >/dev/null 2>&1
	GOT_IT=$?
done

exit $GOT_IT


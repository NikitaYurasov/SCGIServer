#!/bin/bash

for i in {1..6}; do
	wrk -t"$i" -c100 -d10s http://localhost/highloadtesting >> ~/Lebed/laba2/testing.txt
done

for ((i=100; i<=1000; i+=100)); do
	wrk -t2 -c"$i" -d10s http://localhost/highloadtesting >> ~/Lebed/laba2/testing.txt
done

	
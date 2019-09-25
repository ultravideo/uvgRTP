#!/bin/zsh

make clean && make all -j8
nohup nc -kluvw 0 localhost 8888 &> /dev/null &

for ((i = 0; i < 1; ++i)); do
	./main_kvzrtp   &>> results/kvzrtp
	./main_jrtp     &>> results/jrtp
	./main_ccrtp    &>> results/ccrtp
	./main_ffmpeg   &>> results/ffmpeg
	./main_live555  &>> results/live555
	./main_ortp     &>> results/ortp
done

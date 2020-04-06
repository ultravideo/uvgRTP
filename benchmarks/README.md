# Benchmarks

Send benchmark is the coordinator in the benchmarks so it must be started first.

## Example 1

Run the benchmark one time with 5 threads and sleep 3ms between frames

```
./benchmark.pl \
   --lib kvzrtp \
   --role send \
   --addr 10.21.25.2 \
   --port 9999 \
   --iter 100 \
   --threads 5 \
   --sleep 3000
```

## Example 2

Run the benchmark with 3..1 threads 10 times, first sleeping 2.1ms between frames, then 2.2ms etc.

```
./benchmark.pl \
   --lib kvzrtp \
   --role send \
   --addr 10.21.25.2 \
   --port 9999 \
   --iter 100 \
   --threads 2 \
   --start 2000 \
   --end 3000 \
   --step 100
```

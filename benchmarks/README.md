# Benchmarks

Send benchmark is the coordinator in the benchmarks so it must be started first.

## Example 3

Benchmark uvgRTP's send goodput using netcat

Sender
```
./benchmark.pl \
   --lib uvgrtp \
   --role send \
   --use-nc \
   --addr 127.0.0.1 \
   --port 9999 \
   --threads 3 \
   --start 30 \
   --end 60 \
```

Receiver
```
./benchmark.pl \
   --lib uvgrtp \
   --role recv \
   --use-nc \
   --addr 127.0.0.1 \
   --port 9999 \
   --threads 3 \
   --start 30 \
   --end 60 \
```

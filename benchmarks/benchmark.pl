#!/usr/bin/env perl

use warnings;
use strict;
use IO::Socket;
use IO::Socket::INET;
use Getopt::Long;

$| = 1; # autoflush

sub clamp {
    my ($start, $end) = @_;
    my @clamped = (0, 0);

    $clamped[0] = $start < 15   ? 15   : $start;
    $clamped[1] = $end   > 1500 ? 1500 : $end;

    return @clamped;
}

sub send_benchmark {
    my ($lib, $addr, $port, $iter, $threads, $start, $end, $gen_recv) = @_;
    my ($sfps, $efps) = clamp($start, $end);
    my ($socket, $remote, $data);

    $socket = IO::Socket::INET->new(
        LocalAddr => $addr,
        LocalPort => $port,
        Proto     => "tcp",
        Type      => SOCK_STREAM,
        Listen    => 1,
    ) or die "Couldn't connect to $addr:$port : $@\n";

    $remote = $socket->accept();

    while ($threads ne 0) {
        for (my $i = $sfps; $i <= $efps; $i *= 2) {
            my $logname = "send_results_$threads" . "threads_$i". "fps_$iter" . "iter";
            for ((1 .. $iter)) {
                $remote->recv($data, 16);
                system ("time ./$lib/sender $addr $threads $i >> $lib/results/$logname 2>&1");
                $remote->send("end") if $gen_recv;
            }
        }

        $threads--;
    }
}

sub recv_benchmark {
    my ($lib, $addr, $port, $iter, $threads, $start, $end) = @_;
    my ($sfps, $efps) = clamp($start, $end);

    my $socket = IO::Socket::INET->new(
        PeerAddr  => $addr,
        PeerPort  => $port,
        Proto     => "tcp",
        Type      => SOCK_STREAM,
        Timeout   => 1,
    ) or die "Couldn't connect to $addr:$port : $@\n";

    while ($threads ne 0) {
        for (my $i = $sfps; $i <= $efps; $i *= 2) {
            my $logname = "recv_results_$threads" . "threads_$i". "fps_$iter" . "iter";
            for ((1 .. $iter)) {
                $socket->send("start");
                system ("time ./$lib/receiver $addr $threads >> $lib/results/$logname 2>&1");
            }
        }

        $threads--;
    }
}

# use netcat to capture the stream
sub recv_generic {
    my ($lib, $addr, $port, $iter, $threads, $start, $end) = @_;
    my ($sfps, $efps) = clamp($start, $end);
    my $ports = "";

    my $socket = IO::Socket::INET->new(
        PeerAddr  => $addr,
        PeerPort  => $port,
        Proto     => "tcp",
        Type      => SOCK_STREAM,
        Timeout   => 1,
    ) or die "Couldn't connect to $addr:$port : $@\n";

    # spawn N netcats using gnu parallel, send message to sender to start sending,
    # wait for message from sender that all the packets have been sent, sleep a tiny bit
    # move receiver output from separate files to one common file and proceed to next iteration
    $ports .= (8888 + $_ * 2) . " " for ((0 .. $threads - 1));

    while ($threads ne 0) {
        for (my $i = $sfps; $i <= $efps; $i *= 2) {
            my $logname = "recv_results_$threads" . "threads_$i". "fps";
            system "parallel --files nc -kluvw 0 $addr ::: $ports &";
            $socket->send("start");
            $socket->recv(my $data, 16);
            sleep 1;
            system "killall nc";

            open my $fhz, '>>', "$lib/results/$logname";
            opendir my $dir, "/tmp";

            foreach my $of (grep (/par.+\.par/i, readdir $dir)) {
                print $fhz -s "/tmp/$of";
                print $fhz "\n";
                unlink "/tmp/$of";
            }
            closedir $dir;
        }

        $threads--;
    }
}

sub print_help {
    print "usage (benchmark):\n  ./benchmark.pl \n"
    . "\t--lib <uvgrtp|ffmpeg|gstreamer>\n"
    . "\t--role <send|recv>\n"
    . "\t--addr <server address>\n"
    . "\t--port <server port>\n"
    . "\t--threads <# of threads>\n"
    . "\t--start <start fps>\n"
    . "\t--end <end fps>\n" and exit;
}
    
GetOptions(
    "lib|l=s"     => \(my $lib = ""),
    "role|r=s"    => \(my $role = ""),
    "addr|a=s"    => \(my $addr = ""),
    "port|p=i"    => \(my $port = 0),
    "iter|i=i"    => \(my $iter = 10),
    "threads|t=i" => \(my $threads = 1),
    "start|s=f"   => \(my $start = 0),
    "end|e=f"     => \(my $end = 0),
    "use-nc"      => \(my $nc = 0),
    "help"        => \(my $help = 0)
) or die "failed to parse command line!\n";

print_help() if $help;
print_help() if !$lib or !$addr or !$port;
print_help() if !$start or !$end;

if ($role eq "send") {
    system ("make $lib" . "_sender");
    send_benchmark($lib, $addr, $port, $iter, $threads, $start, $end, $nc);
} elsif ($role eq "recv" ) {
    if (!$nc) {
        system ("make $lib" . "_receiver");
        recv_benchmark($lib, $addr, $port, $iter, $threads, $start, $end);
    } else {
        recv_generic($lib, $addr, $port, $iter, $threads, $start, $end);
    }
} else {
    print "invalid role: '$role'\n" and exit;
}

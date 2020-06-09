#!/usr/bin/env perl

use warnings;
use strict;
use IO::Socket;
use IO::Socket::INET;
use Getopt::Long;

$| = 1; # autoflush

my $DEFAULT_ADDR = "10.21.25.200";
my $DEFAULT_PORT = 9999;

sub clamp {
    my ($start, $end) = @_;
    my @clamped = (0, 0);

    $clamped[0] = $start < 30   ? 30   : $start;
    $clamped[1] = $end   > 5000 ? 5000 : $end;

    return @clamped;
}

sub mk_ssock {
    my $s = IO::Socket::INET->new(
        LocalAddr => $_[0],
        LocalPort => $_[1],
        Proto     => "tcp",
        Type      => SOCK_STREAM,
        Listen    => 1,
    ) or die "Couldn't connect to $_[0]:$_[1]: $@\n";

    return $s;
}

sub mk_rsock {
    my $s = IO::Socket::INET->new(
        PeerAddr  => $_[0],
        PeerPort  => $_[1],
        Proto     => "tcp",
        Type      => SOCK_STREAM,
        Timeout   => 1,
    ) or die "Couldn't connect to $_[0]:$_[1]: $@\n";

    return $s;
}

sub send_benchmark {
    my ($lib, $addr, $port, $iter, $threads, $gen_recv, $mode, $e, @fps_vals) = @_;
    my ($socket, $remote, $data);
    my @execs = split ",", $e;

    $socket = mk_ssock($addr, $port);
    $remote = $socket->accept();

    foreach (@execs) {
        my $exec = $_;
        foreach ((1 .. $threads)) {
            my $thread = $_;
            foreach (@fps_vals) {
                my $fps = $_;
                my $logname = "send_results_$thread" . "threads_$fps". "fps_$iter" . "iter_$exec";

                for ((1 .. $iter)) {
                    $remote->recv($data, 16);
                    system ("time ./$lib/$exec $addr $thread $fps $mode >> $lib/results/$logname 2>&1");
                    $remote->send("end") if $gen_recv;
                }
            }
        }
    }
}

sub recv_benchmark {
    my ($lib, $addr, $port, $iter, $threads, $e, @fps_vals) = @_;
    my $socket = mk_rsock($addr, $port);
    my @execs = split ",", $e;

    foreach (@execs) {
        my $exec = $_;
        foreach ((1 .. $threads)) {
            my $thread = $_;
            foreach (@fps_vals) {
                my $logname = "recv_results_$thread" . "threads_$_". "fps_$iter" . "iter_$exec";
                for ((1 .. $iter)) {
                    $socket->send("start");
                    system ("time ./$lib/receiver $addr $thread >> $lib/results/$logname 2>&1");
                }
            }
        }
    }
}

# use netcat to capture the stream
sub recv_generic {
    my ($lib, $addr, $port, $iter, $threads, @fps_vals) = @_;
    # my ($sfps, $efps) = clamp($start, $end);
    my $socket = mk_rsock($addr, $port);
    my $ports = "";

    # spawn N netcats using gnu parallel, send message to sender to start sending,
    # wait for message from sender that all the packets have been sent, sleep a tiny bit
    # move receiver output from separate files to one common file and proceed to next iteration
    $ports .= (8888 + $_ * 2) . " " for ((0 .. $threads - 1));

    while ($threads ne 0) {
        foreach (@fps_vals) {
            my $logname = "recv_results_$threads" . "threads_$_". "fps";
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

sub lat_send {
    my ($lib, $addr, $port) = @_;
    my ($socket, $remote, $data);

    $socket = mk_ssock($addr, $port);
    $remote = $socket->accept();

    for ((1 .. 100)) {
        $remote->recv($data, 16);
        system ("./$lib/latency_sender >> $lib/results/latencies 2>&1");
    }
}

sub lat_recv {
    my ($lib, $addr, $port) = @_;
    my $socket = mk_rsock($addr, $port);

    for ((1 .. 100)) {
        $socket->send("start");
        system ("./$lib/latency_receiver 2>&1 >/dev/null");
        sleep 2;
    }
}

# TODO explain every parameter
sub print_help {
    print "usage (benchmark):\n  ./benchmark.pl \n"
    . "\t--lib <uvgrtp|ffmpeg|live555>\n"
    . "\t--role <send|recv>\n"
    . "\t--addr <server address>\n"
    . "\t--port <server port>\n"
    . "\t--threads <# of threads>\n"
    . "\t--mode <strict|best-effort>\n"
    . "\t--start <start fps>\n"
    . "\t--end <end fps>\n\n";

    print "usage (latency):\n  ./benchmark.pl \n"
    . "\t--latency\n"
    . "\t--role <send|recv>\n"
    . "\t--addr <server address>\n"
    . "\t--port <server port>\n"
    . "\t--lib <uvgrtp|ffmpeg|live555>\n\n" and exit;
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
    "step=i"      => \(my $step = 0),
    "use-nc"      => \(my $nc = 0),
    "fps=s"       => \(my $fps = ""),
    "latency"     => \(my $lat = 0),
    "mode=s"      => \(my $mode = "best-effort"),
    "exec=s"      => \(my $exec = "default"),
    "help"        => \(my $help = 0)
) or die "failed to parse command line!\n";

$port = $DEFAULT_PORT if !$port;
$addr = $DEFAULT_ADDR if !$addr;

print_help() if $help or !$lib;
print_help() if ((!$start or !$end) and !$fps) and !$lat;
print_help() if not grep /$mode/, ("strict", "best-effort");

die "not implemented\n" if !grep (/$lib/, ("uvgrtp", "ffmpeg", "live555"));
my @fps_vals = ();

if (!$lat) {
    if ($fps) {
        @fps_vals = split ",", $fps;
    } else {
        ($start, $end) = clamp($start, $end);
        for (my $i = $start; $i <= $end; ) {
            push @fps_vals, $i;

            if ($step) { $i += $step; }
            else       { $i *= 2; }
        }
    }
}

if ($role eq "send") {
    if ($lat) {
        system "make $lib" . "_latency_sender";
        lat_send($lib, $addr, $port);
    } else {
        if ($exec eq "default") {
            system "make $lib" . "_sender";
            $exec = "sender";
        }
        send_benchmark($lib, $addr, $port, $iter, $threads, $nc, $mode, $exec, @fps_vals);
    }
} elsif ($role eq "recv" ) {
    if ($lat) {
        system "make $lib" . "_latency_receiver";
        lat_recv($lib, $addr, $port);
    } elsif (!$nc) {
        if ($exec eq "default") {
            system "make $lib" . "_receiver";
            $exec = "receiver";
        }
        recv_benchmark($lib, $addr, $port, $iter, $threads, $exec, @fps_vals);
    } else {
        recv_generic($lib, $addr, $port, $iter, $threads, @fps_vals);
    }
} else {
    print "invalid role: '$role'\n" and exit;
}

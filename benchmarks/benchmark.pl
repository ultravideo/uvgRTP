#!/usr/bin/env perl

use warnings;
use strict;
use IO::Socket;
use IO::Socket::INET;
use Getopt::Long;

$| = 1; # autoflush

sub send_benchmark {
	my ($lib, $addr, $port, $iter, $threads, $start, $end, $step) = @_;
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
		for (my $i = $start; $i <= $end; $i += $step) {
			my $logname = "send_results_$threads" . "threads_$i". "us";
			for ((1 .. $iter)) {
				$remote->recv($data, 16);
				system ("time ./$lib/sender $threads $i >> $lib/results/$logname 2>&1");
			}
		}

		$threads--;
	}
}

sub recv_benchmark {
	my ($lib, $addr, $port, $iter, $threads, $start, $end, $step) = @_;

	my $socket = IO::Socket::INET->new(
		PeerAddr  => $addr,
		PeerPort  => $port,
		Proto     => "tcp",
		Type      => SOCK_STREAM,
		Timeout   => 1,
	) or die "Couldn't connect to $addr:$port : $@\n";

	while ($threads ne 0) {
		for (my $i = $start; $i <= $end; $i += $step) {
			my $logname = "recv_results_$threads" . "threads_$i". "us";
			for ((1 .. $iter)) {
				$socket->send("start");
				system ("time ./$lib/receiver $threads >> $lib/results/$logname 2>&1");
			}
		}

		$threads--;
	}
}

GetOptions(
	"lib=s"     => \(my $lib = ""),
	"role=s"    => \(my $role = ""),
	"addr=s"    => \(my $addr = ""),
	"port=i"    => \(my $port = 0),
	"iter=i"    => \(my $iter = 100),
	"sleep=i"   => \(my $sleep = 0),
	"threads=i" => \(my $threads = 1),
	"start=i"   => \(my $start = 0),
	"end=i"     => \(my $end = 0),
	"step=i"    => \(my $step = 0)
) or die "failed to parse command line!\n";

if ($lib eq "") {
	print "library not defined!\n" and exit;
}

if ($sleep ne 0) {
	if ($start ne 0 or $end ne 0 or $step ne 0) {
		print "start/end/step and sleep are mutually exclusive\n" and exit;
	}

	$start = $sleep;
	$end   = $sleep + 1;
	$step  = 1;
}

if ($addr eq "" or $port eq 0) {
	print "address and port must be defined!\n" and exit;
}

if ($role eq "send") {
	system ("make $lib" . "_sender");
	send_benchmark($lib, $addr, $port, $iter, $threads, $start, $end, $step);
} elsif ($role eq "recv" ){
	system ("make $lib" . "_receiver");
	recv_benchmark($lib, $addr, $port, $iter, $threads, $start, $end, $step);
} else {
	print "invalid role: '$role'\n" and exit;
}

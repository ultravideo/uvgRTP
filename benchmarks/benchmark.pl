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

	$clamped[0] = $start < 0.9375 ? 0.9375 : $start;
	$clamped[1] = $end   > 960    ? 960    : $end;

	return @clamped;
}

sub send_benchmark {
	my ($lib, $addr, $port, $iter, $threads, $start, $end) = @_;
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
			my $logname = "send_results_$threads" . "threads_$i". "fps";
			for ((1 .. $iter)) {
				$remote->recv($data, 16);
				system ("time ./$lib/sender $addr $threads $i >> $lib/results/$logname 2>&1");
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
			my $logname = "recv_results_$threads" . "threads_$i". "fps";
			for ((1 .. $iter)) {
				$socket->send("start");
				system ("time ./$lib/receiver $addr $threads >> $lib/results/$logname 2>&1");
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
	"threads=i" => \(my $threads = 1),
	"start=f"   => \(my $start = 0),
	"end=f"     => \(my $end = 0),
) or die "failed to parse command line!\n";

if ($lib eq "") {
	print "library not defined!\n" and exit;
}

if (!$start or !$end) {
	print "start and end FPS values must be defined!\n" and exit;
}

if ($addr eq "" or $port eq 0) {
	print "address and port must be defined!\n" and exit;
}

if ($role eq "send") {
	system ("make $lib" . "_sender");
	send_benchmark($lib, $addr, $port, $iter, $threads, $start, $end);
} elsif ($role eq "recv" ){
	system ("make $lib" . "_receiver");
	recv_benchmark($lib, $addr, $port, $iter, $threads, $start, $end);
} else {
	print "invalid role: '$role'\n" and exit;
}

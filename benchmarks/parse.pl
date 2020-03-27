#!/usr/bin/env perl

use warnings;
use strict;

# extract average send/receive goodputs
# and calculate averages for the time command outputs
sub parse_kvzrtp_recv {
	my ($path) = @_;

	open(my $fh, '<', $path) or die "failed to open file";

	my ($t_usr, $t_sys, $t_cpu, $t_total, $t_time, $t_bytes, $lines);

	# each iteration parses one benchmark run
	while (my $line = <$fh>) {
		$line = <$fh>;
		my ($bytes, $time) = ($line =~ m/(\d+)\s(\d+)/);

		$line = <$fh>;
		my ($usr, $sys, $total, $cpu) = ($line =~ m/(\d+\.\d+)user\s(\d+\.\d+)system\s0:(\d+.\d+)elapsed\s(\d+)%CPU/);

		# discard line about inputs, outputs and pagefaults
		$line = <$fh>;

		# update total
		$t_usr   += $usr;
		$t_sys   += $sys;
		$t_cpu   += $cpu;
		$t_total += $total;
		$t_time  += $time;
		$t_bytes += $bytes;
		$lines   += 1;
	}

	my $gp = int((($t_bytes / $lines) / 1000 / 1000 / ($t_time  / $lines)) * 1000);

	print "$path: \n";
	print "\tuser:    " .  $t_usr   / $lines . "\n";
	print "\tsystem:  " .  $t_sys   / $lines . "\n";
	print "\tcpu:     " .  $t_cpu   / $lines . "\n";
	print "\ttotal:   " .  $t_total / $lines . "\n";
	print "\ttime:    " .  $t_time  / $lines . " ms\n";
	print "\tgoodput: " .  $gp . " MB/s\n";

	close $fh;
}

sub parse_kvzrtp_send {
	my ($path) = @_;

	open(my $fh, '<', $path) or die "failed to open file";

	my ($t_usr, $t_sys, $t_cpu, $t_total, $t_time, $t_bytes, $lines);

	# each iteration parses one benchmark run
	while (my $line = <$fh>) {
		$line = <$fh>;
		my @nums = $line =~ /(\d+)/g;

		$line = <$fh>;
		my ($usr, $sys, $total, $cpu) = ($line =~ m/(\d+\.\d+)user\s(\d+\.\d+)system\s0:(\d+.\d+)elapsed\s(\d+)%CPU/);

		# discard line about inputs, outputs and pagefaults
		$line = <$fh>;

		# update total
		$t_usr   += $usr;
		$t_sys   += $sys;
		$t_cpu   += $cpu;
		$t_total += $total;
		$t_bytes += $nums[0];
		$t_time  += $nums[3];
		$lines   += 1;
	}

	my $gp = int((($t_bytes / $lines) / 1000 / 1000 / ($t_time  / $lines)) * 1000);

	print "$path: \n";
	print "\tuser:    " .  $t_usr   / $lines . "\n";
	print "\tsystem:  " .  $t_sys   / $lines . "\n";
	print "\tcpu:     " .  $t_cpu   / $lines . "\n";
	print "\ttotal:   " .  $t_total / $lines . "\n";
	print "\ttime:    " .  $t_time  / $lines . " ms\n";
	print "\tgoodput: " .  $gp . " MB/s\n";

	close $fh;
}

if ($#ARGV + 1 != 3) {
	print "usage: ./benchmarks.pl"
	. "\n\t<kvzrtp|ffmpeg|gstreamer>"
	. "\n\t<send|recv>"
	. "\n\t<path to log file>\n" and exit;
}

my ($lib, $role, $path) = @ARGV;

if ($lib eq "kvzrtp") {
	if ($role eq "send") {
		parse_kvzrtp_send($path);
	} else {
		parse_kvzrtp_recv($path);
	}
} elsif ($lib eq "ffmpeg") {
	die "not implemented";
} elsif ($lib eq "gstreamer") {
	die "not implemented";
} else {
	die "unknown lib: $lib";
}

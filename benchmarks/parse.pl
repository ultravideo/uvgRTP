#!/usr/bin/env perl

use warnings;
use strict;
use Getopt::Long;

my $TOTAL_BYTES = 411410113;

sub parse_kvzrtp_send {
    my ($threads, $path) = @_;

    open(my $fh, '<', $path) or die "failed to open file";

    my ($t_usr, $t_sys, $t_cpu, $t_total, $t_time);
    my ($t_sgp, $t_tgp, $lines);

    # each iteration parses one benchmark run
    # and each benchmark run can have 1..N entries, one for each thread
	while (my $line = <$fh>) {
        my $rt_avg = 0;
        my $rb_avg = 0;

        # for multiple threads there are two numbers:
        #  - single thread performance
        #     -> for each thread, calculate the speed at which the data was sent,
        #        sum all those together and divide by the number of threads
        #
        #  - total performance
        #     -> (amount of data * number of threads) / total time spent
        #
        for (my $i = 0; $i < $threads; $i++) {
            my @nums = $line =~ /(\d+)/g;
            $rt_avg += $nums[3];
            $line = <$fh>;
        }
        $rt_avg /= $threads;

        my $gp = ($TOTAL_BYTES / 1000 / 1000) / $rt_avg * 1000;

		my ($usr, $sys, $total, $cpu) = ($line =~ m/(\d+\.\d+)user\s(\d+\.\d+)system\s0:(\d+.\d+)elapsed\s(\d+)%CPU/);

		# discard line about inputs, outputs and pagefaults
		$line = <$fh>;

        # update total
        $t_usr   += $usr;
        $t_sys   += $sys;
        $t_cpu   += $cpu;
        $t_total += $total;
        $t_sgp   += $gp;
		$lines   += 1;
	}

    $t_sgp = int($t_sgp / $lines);

    if ($threads gt 1) {
        $t_tgp = int((($TOTAL_BYTES / 1000 / 1000) * 8) / (($t_total / $lines) * 1000) * 1000);
    } else {
        $t_sgp = int($t_sgp / $lines);
    }

	print "$path: \n";
	print "\tuser:            " . $t_usr   / $lines . "\n";
	print "\tsystem:          " . $t_sys   / $lines . "\n";
	print "\tcpu:             " . $t_cpu   / $lines . "\n";
	print "\ttotal:           " . $t_total / $lines . "\n";
    print "\tgoodput, single: " . $t_sgp            . " MB/s\n";
    print "\tgoodput, total:  " . $t_tgp            . " MB/s\n";

	close $fh;
}

sub parse_kvzrtp_recv {
    my ($threads, $path) = @_;

    open(my $fh, '<', $path) or die "failed to open file";

	my ($t_usr, $t_sys, $t_cpu, $t_total, $t_time, $t_bytes, $lines);

	# each iteration parses one benchmark run
	while (my $line = <$fh>) {
		$line = <$fh>;
		my @nums = $line =~ /(\d+)/g;
		$t_bytes += $nums[0];
		$t_time  += $nums[1];

		$line = <$fh>;
		my ($usr, $sys, $total, $cpu) = ($line =~ m/(\d+\.\d+)user\s(\d+\.\d+)system\s0:(\d+.\d+)elapsed\s(\d+)%CPU/);

		# discard line about inputs, outputs and pagefaults
		$line = <$fh>;

		# update total
		$t_usr   += $usr;
		$t_sys   += $sys;
		$t_cpu   += $cpu;
		$t_total += $total;
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

GetOptions(
	"lib=s"     => \(my $lib = ""),
	"role=s"    => \(my $role = ""),
    "path=s"    => \(my $path = ""),
	"threads=i" => \(my $threads = 1),
) or die "failed to parse command line!\n";

if ($lib eq "kvzrtp") {
    if ($role eq "send") {
	    parse_kvzrtp_send($threads, $path);
    } else {
	    parse_kvzrtp_recv($threads, $path);
    }
} elsif ($lib eq "ffmpeg") {
	die "not implemented";
} elsif ($lib eq "gstreamer") {
	die "not implemented";
}

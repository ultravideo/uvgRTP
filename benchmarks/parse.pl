#!/usr/bin/env perl

use warnings;
use strict;
use Getopt::Long;

my $TOTAL_FRAMES = 602;
my $TOTAL_BYTES  = 411410113;

# open the file, validate it and return file handle to caller
sub open_file {
    my ($path, $expect) = @_;
    my $lines  = 0;

    open(my $fh, '<', $path) or die "failed to open file";
    $lines++ while (<$fh>);

    if ($lines != $expect) {
        print "invalid file: $path ($lines != $expect)!\n" and exit;
    }

    seek $fh, 0, 0;
    return $fh;
}

sub parse_uvgrtp_send {
    my ($iter, $threads, $path) = @_;

    my ($t_usr, $t_sys, $t_cpu, $t_total, $t_time);
    my ($t_sgp, $t_tgp, $lines);

    my $e  = ($iter * ($threads + 2));
    my $fh = open_file($path, $e);

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
        $t_tgp = $t_sgp;
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

sub parse_uvgrtp_recv {
    my ($iter, $threads, $path) = @_;
    my ($t_usr, $t_sys, $t_cpu, $t_total, $tb_avg, $tf_avg, $tt_avg, $lines);

    my $e  = ($iter * ($threads + 2));
    my $fh = open_file($path, $e);

    # each iteration parses one benchmark run
    while (my $line = <$fh>) {
        my ($a_f, $a_b, $a_t) = (0) x 7;

        # calculate avg bytes/frames/time
        for (my $i = 0; $i < $threads; $i++) {
            my @nums = $line =~ /(\d+)/g;

            $a_b += $nums[0];
            $a_f += $nums[1];
            $a_t += $nums[2];

            $line = <$fh>;
        }

        $tf_avg += ($a_f / $threads);
        $tb_avg += ($a_b / $threads);
        $tt_avg += ($a_t / $threads);

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

    print "$path: \n";
    print "\tuser:       " . $t_usr   / $lines . "\n";
    print "\tsystem:     " . $t_sys   / $lines . "\n";
    print "\tcpu:        " . $t_cpu   / $lines . "\n";
    print "\ttotal:      " . $t_total / $lines . "\n";
    print "\tavg frames: " . (100 * (($tf_avg  / $lines) / $TOTAL_FRAMES)) . "\n";
    print "\tavg bytes:  " . (100 * (($tb_avg  / $lines) / $TOTAL_BYTES))  . "\n";
    print "\tavg time    " . (100 * (($tt_avg  / $lines))) . "\n";

    close $fh;
}

GetOptions(
    "lib=s"     => \(my $lib = ""),
    "role=s"    => \(my $role = ""),
    "path=s"    => \(my $path = ""),
    "threads=i" => \(my $threads = 1),
    "iter=i"    => \(my $iter = 100),
    "help"      => \(my $help = 0)
) or die "failed to parse command line!\n";

if ($help == 1) {
    print "usage: ./parse.pl \n"
    . "\t--lib <uvgrtp|ffmpeg|gstreamer>\n"
    . "\t--role <send|recv>\n"
    . "\t--path <path to log file>\n"
    . "\t--iter <# of iterations> (defaults to 100)\n"
    . "\t--threads <# of threads used in the benchmark> (defaults to 1)\n" and exit;
}

if ($lib eq "uvgrtp") {
    if ($role eq "send") {
        parse_uvgrtp_send($iter, $threads, $path);
    } else {
        parse_uvgrtp_recv($iter, $threads, $path);
    }
} elsif ($lib eq "ffmpeg") {
    die "not implemented";
} elsif ($lib eq "gstreamer") {
    die "not implemented";
}

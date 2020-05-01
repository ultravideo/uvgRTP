#!/usr/bin/env perl

use warnings;
use strict;
use Getopt::Long;
use Cwd qw(realpath);

my $TOTAL_FRAMES_UVGRTP = 602;
my $TOTAL_FRAMES_FFMPEG = 1196;
my $TOTAL_BYTES         = 411410113;

# open the file, validate it and return file handle to caller
sub open_file {
    my ($path, $expect) = @_;
    my $lines  = 0;

    open(my $fh, '<', $path) or die "failed to open file: $path";
    $lines++ while (<$fh>);

    # if ($lines != $expect) {
    #     return undef;
    # }

    seek $fh, 0, 0;
    return $fh;
}

sub parse_send {
    my ($lib, $iter, $threads, $path) = @_;

    my ($t_usr, $t_sys, $t_cpu, $t_total, $t_time);
    my ($t_sgp, $t_tgp, $fh);

    if ($lib eq "uvgrtp") {
        my $e  = ($iter * ($threads + 2));
        $fh = open_file($path, $e);
        return if not defined $fh;
    } else {
        open $fh, '<', $path or die "failed to open file\n";
    }

    # each iteration parses one benchmark run
    # and each benchmark run can have 1..N entries, one for each thread
    START: while (my $line = <$fh>) {
        my $rt_avg = 0;
        my $rb_avg = 0;

        next if index ($line, "kB") == -1 or index ($line, "MB") == -1;

        # for multiple threads there are two numbers:
        #  - single thread performance
        #     -> for each thread, calculate the speed at which the data was sent,
        #        sum all those together and divide by the number of threads
        #
        #  - total performance
        #     -> (amount of data * number of threads) / total time spent
        #
        for (my $i = 0; $i < $threads; $i++) {
            next START if grep /terminated|corrupt/, $line;
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
    }

    $t_sgp = int($t_sgp / $iter);

    if ($threads gt 1) {
        $t_tgp = int((($TOTAL_BYTES / 1000 / 1000) * $threads) / (($t_total / $iter) * 1000) * 1000);
    } else {
        $t_tgp = $t_sgp;
    }

    close $fh;
    return ($path, $t_usr / $iter, $t_sys / $iter, $t_cpu / $iter, $t_total / $iter, $t_sgp, $t_tgp);
}

sub parse_recv {
    my ($lib, $iter, $threads, $path) = @_;
    my ($t_usr, $t_sys, $t_cpu, $t_total, $tb_avg, $tf_avg, $tt_avg, $fh);
    my $tf = ($lib eq "uvgrtp") ? $TOTAL_FRAMES_UVGRTP : $TOTAL_FRAMES_FFMPEG;

    if ($lib eq "uvgrtp") {
        my $e  = ($iter * ($threads + 2));
        $fh = open_file($path, $e);
    } else {
        open $fh, '<', $path or die "failed to open file $path\n";
    }

    # each iteration parses one benchmark run
    while (my $line = <$fh>) {
        my ($a_f, $a_b, $a_t) = (0) x 3;

        # make sure this is a line produced by the benchmarking script before proceeding
        if ($lib eq "ffmpeg") {
            my @nums = $line =~ /(\d+)/g;
            next if $#nums != 2 or grep /jitter/, $line;
        }

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
    }

    close $fh;
    return (
        $path,
        $t_usr / $iter, $t_sys / $iter, $t_cpu / $iter, $t_total / $iter,
        (100 * (($tf_avg  / $iter) / $tf)),
        (100 * (($tb_avg  / $iter) / $TOTAL_BYTES)),
        (100 * (($tt_avg  / $iter)))
    );
}

sub print_recv {
    my ($path, $usr, $sys, $cpu, $total, $a_f, $a_b, $a_t) = parse_recv(@_);

    if (defined $path) {
        print "$path: \n";
        print "\tuser:       $usr  \n";
        print "\tsystem:     $sys  \n";
        print "\tcpu:        $cpu  \n";
        print "\ttotal:      $total\n";
        print "\tavg frames: $a_f\n";
        print "\tavg bytes:  $a_b\n";
        print "\tavg time    $a_t\n";
    }
}

sub print_send {
    my ($path, $usr, $sys, $cpu, $total, $sgp, $tgp) = parse_send(@_);

    if (defined $path) {
        print "$path: \n";
        print "\tuser:            $usr\n";
        print "\tsystem:          $sys\n";
        print "\tcpu:             $cpu\n";
        print "\ttotal:           $total\n";
        print "\tgoodput, single: $sgp MB/s\n";
        print "\tgoodput, total:  $tgp MB/s\n";
    }
}

sub parse_all {
    my ($lib, $iter, $path, $pkt_loss, $frame_loss) = @_;
    my ($tgp, $tgp_k, $sgp, $sgp_k, $threads, $fps, %a) = (0) x 6;
    opendir my $dir, realpath($path);

    foreach my $fh (grep /recv/, readdir $dir) {
        ($threads, $fps) = ($fh =~ /(\d+)threads_(\d+)/g);
        my @values = parse_recv($lib, $iter, $threads, realpath($path) . "/" . $fh);

        if (100.0 - $values[5] <= $frame_loss and 100.0 - $values[6] <= $pkt_loss) {
            $a{"$threads $fps"} = $path;
        }
    }

    rewinddir $dir;

    foreach my $fh (grep /send/, readdir $dir) {
        ($threads, $fps, $fiter) = ($fh =~ /(\d+)threads_(\d+)fps_(\d+)iter/g);
        $iter = $fiter if $fiter;
        print "unable to determine iter, skipping file $fh\n" and next if !$iter;

        my @values = parse_send($lib, $iter, $threads, realpath($path) . "/" . $fh);

        if (exists $a{"$threads $fps"}) {
            if ($values[5] > $sgp) {
                $sgp   = $values[5];
                $sgp_k = $fh;
            }

            if ($values[6] > $tgp) {
                $tgp = $values[6];
                $tgp_k = $fh;
            }
        }
    }

    if ($sgp_k) {
        print "best goodput, single thread: $sgp_k\n";
        ($threads, $fps) = ($sgp_k =~ /(\d+)threads_(\d+)/g);
        print_send($lib, $iter, $threads, realpath($path) . "/" . $sgp_k);
    } else {
        print "nothing found for single best goodput\n";
    }

    if ($tgp_k) {
        print "\nbest goodput, total: $tgp_k\n";
        ($threads, $fps) = ($tgp_k =~ /(\d+)threads_(\d+)/g);
        print_send($lib, $iter, $threads, realpath($path) . "/" . $tgp_k);
    } else {
        print "nothing found for total best goodput\n";
    }

    closedir $dir;
}

sub print_help {
    print "usage (one file):\n  ./parse.pl \n"
    . "\t--lib <uvgrtp|ffmpeg|gstreamer>\n"
    . "\t--role <send|recv>\n"
    . "\t--path <path to log file>\n"
    . "\t--iter <# of iterations>)\n"
    . "\t--threads <# of threads used in the benchmark> (defaults to 1)\n\n";

    print "usage (all files):\n  ./parse.pl \n"
    . "\t--best\n"
    . "\t--lib <uvgrtp|ffmpeg|gstreamer>\n"
    . "\t--iter <# of iterations>)\n"
    . "\t--packet-loss <allowed percentage of dropped packets> (optional)\n"
    . "\t--frame-loss <allowed percentage of dropped frames> (optional)\n"
    . "\t--path <path to folder with send and recv output files>\n" and exit;
}

GetOptions(
    "lib=s"         => \(my $lib = ""),
    "role=s"        => \(my $role = ""),
    "path=s"        => \(my $path = ""),
    "threads=i"     => \(my $threads = 0),
    "iter=i"        => \(my $iter = 0),
    "best"          => \(my $best = 0),
    "packet-loss=f" => \(my $pkt_loss = 100.0),
    "frame-loss=f"  => \(my $frame_loss = 100.0),
    "help"          => \(my $help = 0)
) or die "failed to parse command line!\n";

if (!$lib and $path =~ m/.*(uvgrtp|ffmpeg|gstreamer).*/i) {
    $lib = $1;
}

if (!$role and $path =~ m/.*(recv|send).*/i) {
    $role = $1;
}

if (!$threads and $path =~ m/.*_(\d+)threads.*/i) {
    $threads = $1;
}

if (!$iter and $path =~ m/.*_(\d+)iter.*/i) {
    $iter = $1;
}

print_help() if $help or !$lib;
print_help() if !$iter and !$best and !$csv;
print_help() if (!$best and !$csv and (!$role or !$threads));

my @libs = ("uvgrtp", "ffmpeg");

if (grep (/$lib/, @libs)) {
    if ($best) {
        parse_all($lib, $iter, $path, $pkt_loss, $frame_loss);
    } elsif ($role eq "send") {
        print_send($lib, $iter, $threads, $path);
    } else {
        print_recv($lib, $iter, $threads, $path);
    }
} else {
    die "not implemented\n";
}

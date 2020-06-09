#!/usr/bin/env perl

use warnings;
use strict;
use Getopt::Long;
use Cwd qw(realpath);

my $TOTAL_FRAMES_UVGRTP  = 602;
my $TOTAL_FRAMES_LIVE555 = 601;
my $TOTAL_FRAMES_FFMPEG  = 598;
my $TOTAL_BYTES          = 411410113;

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

sub goodput {
    if    ($_[2] eq "mbit") { return ($_[0] / 1000 / 1000) / $_[1] * 8 * 1000; }
    elsif ($_[2] eq "mb")   { return ($_[0] / 1000 / 1000) / $_[1] * 1000;     }
    else                    { return ($_[0] / 1000 / 1000) / $_[1] * 8;        }
}

sub get_frame_count {
    return ($_[0] eq "uvgrtp") ? $TOTAL_FRAMES_UVGRTP :
           ($_[0] eq "ffmpeg") ? $TOTAL_FRAMES_FFMPEG : $TOTAL_FRAMES_LIVE555;
}

sub parse_send {
    my ($lib, $iter, $threads, $path, $unit) = @_;

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

        next START if grep /terminated|corrupt/, $line;
        $line = <$fh> if grep /flush|Command/, $line;
        my ($usr, $sys, $total, $cpu) = ($line =~ m/(\d+\.\d+)user\s(\d+\.\d+)system\s0:(\d+.\d+)elapsed\s(\d+)%CPU/);

        # discard line about inputs, outputs and pagefaults
        $line = <$fh>;

        # update total
        $t_usr   += $usr;
        $t_sys   += $sys;
        $t_cpu   += $cpu;
        $t_total += $total;
        $t_sgp   += goodput($TOTAL_BYTES, $rt_avg, $unit);
    }

    $t_sgp = $t_sgp / $iter;
    $t_tgp = ($threads > 1) ? goodput($TOTAL_BYTES * $threads, $t_total / $iter, $unit) : $t_sgp;

    close $fh;
    return ($path, $t_usr / $iter, $t_sys / $iter, $t_cpu / $iter, $t_total / $iter, $t_sgp, $t_tgp);
}

sub parse_recv {
    my ($lib, $iter, $threads, $path, $unit) = @_;
    my ($t_usr, $t_sys, $t_cpu, $t_total, $tb_avg, $tf_avg, $tt_avg, $fh);
    my $tf = get_frame_count($lib);

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

    my $bytes  = 100 * (($tb_avg  / $iter) / $TOTAL_BYTES);
    my $frames = 100 * (($tf_avg  / $iter) / $tf);
    my $gp     = goodput(($TOTAL_BYTES * ($bytes / 100), ($tt_avg  / $iter)), $unit);

    close $fh;
    return ($path, $t_usr / $iter, $t_sys / $iter, $t_cpu / $iter, $t_total / $iter, $frames, $bytes, $gp);
}

sub print_recv {
    my ($path, $usr, $sys, $cpu, $total, $a_f, $a_b, $a_t) = parse_recv(@_);

    if (defined $path) {
        print "$path: \n";
        print "\tuser:         $usr  \n";
        print "\tsystem:       $sys  \n";
        print "\tcpu:          $cpu  \n";
        print "\ttotal:        $total\n";
        print "\tavg frames:   $a_f\n";
        print "\tavg bytes:    $a_b\n";
        print "\trecv goodput: $a_t\n";
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
        print "\tgoodput, single: $sgp\n";
        print "\tgoodput, total:  $tgp\n";
    }
}

sub parse_csv {
    my ($lib, $iter, $path, $unit) = @_;
    my ($threads, $fps, $ofps, $fiter, %a) = (0) x 4;
    opendir my $dir, realpath($path);

    foreach my $fh (grep /(recv|send)/, readdir $dir) {
        ($threads, $ofps, $fiter) = ($fh =~ /(\d+)threads_(\d+)fps_(\d+)iter/g);
        $iter = $fiter if $fiter;
        print "unable to determine iter, skipping file $fh\n" and next if !$iter;
        $fps = sprintf("%05d", $ofps);
        my @values;

        if (grep /recv/, $fh) {
            @values = parse_recv($lib, $iter, $threads, realpath($path) . "/" . $fh, $unit);
            shift @values;

            if (not exists $a{"$threads $fps"}) {
                $a{"$threads $fps"} = join(" ", @values);
            } else {
                $a{"$threads $fps"} = join(" ", @values) . " " . $a{"$threads $fps"};
            }

        } else {
            @values = parse_send($lib, $iter, $threads, realpath($path) . "/" . $fh, $unit);
            shift @values;

            if (not exists $a{"$threads $fps"}) {
                $a{"$threads $fps"} = join(" ", @values) . " $ofps";
            } else {
                $a{"$threads $fps"} = $a{"$threads $fps"} . " " . join(" ", @values) . " $ofps";
            }
        }
    }

    my $c_key = 0;
    open my $cfh, '>', "$lib.csv" or die "failed to open file: $lib.csv";
    my (@r_u, @r_s, @r_c, @r_t, @r_f, @r_b, @r_m) = () x 7;
    my (@s_u, @s_s, @s_c, @s_t, @s_sg, @s_tg, @s_f) = () x 7;

    foreach my $key (sort(keys %a)) {
        my $spz = (split " ", $key)[0];

        if ($spz != $c_key){
            if ($spz ne 1) {
                print $cfh "recv usr;"       . join(";", @r_u)  . "\n";
                print $cfh "recv sys;"       . join(";", @r_s)  . "\n";
                print $cfh "recv cpu;"       . join(";", @r_c)  . "\n";
                print $cfh "recv total;"     . join(";", @r_t)  . "\n";
                print $cfh "frames received;". join(";", @r_f)  . "\n";
                print $cfh "bytes received;" . join(";", @r_b)  . "\n";
                print $cfh "time estimate;"  . join(";", @r_m)  . "\n";
                print $cfh "send usr;"       . join(";", @s_u)  . "\n";
                print $cfh "send sys;"       . join(";", @s_s)  . "\n";
                print $cfh "send cpu;"       . join(";", @s_c)  . "\n";
                print $cfh "send total;"     . join(";", @s_t)  . "\n";
                print $cfh "single goodput;" . join(";", @s_sg) . "\n";
                print $cfh "total goodput;"  . join(";", @s_tg) . "\n";
                print $cfh "fps;"            . join(";", @s_f)  . "\n\n";
            }

            print $cfh "$spz threads;\n";
            $c_key = $spz;
            (@r_f, @r_b, @r_m, @r_c, @r_u, @r_s, @r_t) = () x 7;
            (@s_c, @s_u, @s_s, @s_t, @s_sg, @s_tg, @s_f) = () x 7;
        }

        my @comp = split " ", $a{$key};
        push @r_u,  $comp[0];  push @r_s, $comp[1];  push @r_c,  $comp[2];
        push @r_t,  $comp[3];  push @r_f, $comp[4];  push @r_b,  $comp[5];
        push @r_m,  $comp[6];  push @s_u, $comp[7];  push @s_s,  $comp[8];
        push @s_c,  $comp[9];  push @s_t, $comp[10]; push @s_sg, $comp[11];
        push @s_tg, $comp[12]; push @s_f, $comp[13];
    }

    print $cfh "recv usr;"       . join(";", @r_u)  . "\n";
    print $cfh "recv sys;"       . join(";", @r_s)  . "\n";
    print $cfh "recv cpu;"       . join(";", @r_c)  . "\n";
    print $cfh "recv total;"     . join(";", @r_t)  . "\n";
    print $cfh "frames received;". join(";", @r_f)  . "\n";
    print $cfh "bytes received;" . join(";", @r_b)  . "\n";
    print $cfh "recv goodput;"   . join(";", @r_m)  . "\n";
    print $cfh "send usr;"       . join(";", @s_u)  . "\n";
    print $cfh "send sys;"       . join(";", @s_s)  . "\n";
    print $cfh "send cpu;"       . join(";", @s_c)  . "\n";
    print $cfh "send total;"     . join(";", @s_t)  . "\n";
    print $cfh "single goodput;" . join(";", @s_sg) . "\n";
    print $cfh "total goodput;"  . join(";", @s_tg) . "\n";
    print $cfh "fps;"            . join(";", @s_f)  . "\n";

    close $cfh;
}

sub parse {
    my ($lib, $iter, $path, $pkt_loss, $frame_loss, $type, $unit) = @_;
    my ($tgp, $tgp_k, $sgp, $sgp_k, $threads, $fps, $fiter, %a) = (0) x 7;
    opendir my $dir, realpath($path);

    foreach my $fh (grep /recv/, readdir $dir) {
        ($threads, $fps, $fiter) = ($fh =~ /(\d+)threads_(\d+)fps_(\d+)iter/g);
        $iter = $fiter if $fiter;
        print "unable to determine iter, skipping file $fh\n" and next if !$iter;

        my @values = parse_recv($lib, $iter, $threads, realpath($path) . "/" . $fh, $unit);

        if (100.0 - $values[5] <= $frame_loss and 100.0 - $values[6] <= $pkt_loss) {
            $a{"$threads $fps"} = $path;
        }
    }

    rewinddir $dir;

    foreach my $fh (grep /send/, readdir $dir) {
        ($threads, $fps, $fiter) = ($fh =~ /(\d+)threads_(\d+)fps_(\d+)iter/g);
        $iter = $fiter if $fiter;
        print "unable to determine iter, skipping file $fh\n" and next if !$iter;

        my @values = parse_send($lib, $iter, $threads, realpath($path) . "/" . $fh, $unit);

        if (exists $a{"$threads $fps"}) {
            if ($type eq "best") {
                if ($values[5] > $sgp) {
                    $sgp   = $values[5];
                    $sgp_k = $fh;
                }

                if ($values[6] > $tgp) {
                    $tgp = $values[6];
                    $tgp_k = $fh;
                }
            } else {
                print "$fh: $values[5] $values[6]\n" if exists $a{"$threads $fps"};
            }
        }
    }

    closedir $dir;
    exit if $type eq "all";

    if ($sgp_k) {
        print "best goodput, single thread: $sgp_k\n";
        ($threads, $fps) = ($sgp_k =~ /(\d+)threads_(\d+)/g);
        print_send($lib, $iter, $threads, realpath($path) . "/" . $sgp_k, $unit);
    } else {
        print "nothing found for single best goodput\n";
    }

    if ($tgp_k) {
        print "\nbest goodput, total: $tgp_k\n";
        ($threads, $fps) = ($tgp_k =~ /(\d+)threads_(\d+)/g);
        print_send($lib, $iter, $threads, realpath($path) . "/" . $tgp_k, $unit);
    } else {
        print "nothing found for total best goodput\n";
    }
}

sub parse_latency {
    my ($lib, $iter, $path, $unit) = @_;
    my ($ts, $avg, $intra, $inter, $cnt) = (0) x 5;

    open my $fh, '<', $path or die "failed to open file $path\n";

    # each iteration parses one benchmark run
    while (my $line = <$fh>) {
        my @nums = ($line =~ m/(\d+).*intra\s(\d+\.\d+).*inter\s(\d+\.\d+).*avg\s(\d+\.\d+)/);

        $frames += $nums[0];
        $intra  += $nums[1];
        $inter  += $nums[2];
        $avg    += $nums[3];
        $cnt    += 1;
    }

    $intra  /= $cnt;
    $inter  /= $cnt;
    $avg    /= $cnt;
    $frames /= get_frame_count($lib);

    print "$frames: intra $intra, inter $inter, avg $avg\n";
}

sub print_help {
    print "usage (one file, send/recv):\n  ./parse.pl \n"
    . "\t--lib <uvgrtp|ffmpeg|live555>\n"
    . "\t--role <send|recv>\n"
    . "\t--unit <mb|mbit|gbit> (defaults to mb)\n"
    . "\t--path <path to log file>\n"
    . "\t--iter <# of iterations>)\n"
    . "\t--threads <# of threads used in the benchmark> (defaults to 1)\n\n";

    print "usage (latency):\n  ./parse.pl \n"
    . "\t--unit <mb|mbit|gbit> (defaults to mb)\n"
    . "\t--path <path to log file>\n"
    . "\t--parse latency\n\n";

    print "usage (directory):\n  ./parse.pl \n"
    . "\t--parse <best|all|csv>\n"
    . "\t--lib <uvgrtp|ffmpeg|live555>\n"
    . "\t--iter <# of iterations>)\n"
    . "\t--unit <mb|mbit|gbit> (defaults to mb)\n"
    . "\t--packet-loss <allowed percentage of dropped packets> (optional)\n"
    . "\t--frame-loss <allowed percentage of dropped frames> (optional)\n"
    . "\t--path <path to folder with send and recv output files>\n" and exit;
}

GetOptions(
    "lib|l=s"         => \(my $lib = ""),
    "role|r=s"        => \(my $role = ""),
    "path|p=s"        => \(my $path = ""),
    "threadst|=i"     => \(my $threads = 0),
    "iter|i=i"        => \(my $iter = 0),
    "parse|s=s"       => \(my $parse = ""),
    "packet-loss|p=f" => \(my $pkt_loss = 100.0),
    "frame-loss|f=f"  => \(my $frame_loss = 100.0),
    "unit=s"          => \(my $unit = "mb"),
    "help"            => \(my $help = 0)
) or die "failed to parse command line!\n";

$lib     = $1 if (!$lib     and $path =~ m/.*(uvgrtp|ffmpeg|live555).*/i);
$role    = $1 if (!$role    and $path =~ m/.*(recv|send).*/i);
$threads = $1 if (!$threads and $path =~ m/.*_(\d+)threads.*/i);
$iter    = $1 if (!$iter    and $path =~ m/.*_(\d+)iter.*/i);

print_help() if $help or (!$lib and $parse ne "latency");
print_help() if !$iter and !$parse;
print_help() if !$parse and (!$role or !$threads);
print_help() if !grep /$unit/, ("mb", "mbit", "gbit");

die "not implemented\n" if !grep (/$lib/, ("uvgrtp", "ffmpeg", "live555"));

if ($parse eq "best" or $parse eq "all") {
    parse($lib, $iter, $path, $pkt_loss, $frame_loss, $parse, $unit);
} elsif ($parse eq "csv") {
    parse_csv($lib, $iter, $path, $unit);
} elsif ($parse eq "latency") {
    parse_latency($lib, $iter, $path, $unit);
} elsif ($role eq "send") {
    print_send($lib, $iter, $threads, $path, $unit);
} elsif ($role eq "recv") {
    print_recv($lib, $iter, $threads, $path, $unit);
} else {
    die "unknown option!\n";
}

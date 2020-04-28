#!/bin/bash

if [[ "$#" -ne 4 ]]; then
    echo "usage: ./find_best.sh <uvgrtp|ffmpeg|gstreamer> <send|recv> <iter> <path to folder with results>"
    exit;
fi

if [[ $2 = "recv" || $2 = "send" ]]; then
    for f in $(ls $4 | grep $2); do
        ./parse.pl \
            --threads $(echo "$f" | egrep -o "([0-9]+)" | head -n1) \
            --iter "$3" \
            --path "$4/$f" \
            --role $2 \
            --lib $1 2>&1 >> /tmp/results_out
    done

    if [ $2 = "recv" ]; then
		fu_res=$(grep "avg frames" </tmp/results_out | sort -Vr | head -n1)
		bu_res=$(grep "avg bytes" </tmp/results_out | sort -Vr | head -n1)
        f_res=$(grep "$fu_res" -B 6 </tmp/results_out)
        b_res=$(grep "$bu_res" -B 6 </tmp/results_out)
    else
		fu_res=$(grep "goodput, total" </tmp/results_out | sort -Vr | head -n1)
		bu_res=$(grep "goodput, single" </tmp/results_out | sort -Vr | head -n1)
        f_res=$(grep "$fu_res" -B 6 </tmp/results_out)
        b_res=$(grep "$bu_res" -B 6 </tmp/results_out)
    fi

    f_config=$(echo "$f_res" | grep $1)
    b_config=$(echo "$b_res" | grep $1)
    f_thr=$(echo "$f_config" | egrep -o "([0-9]+)" | head -n1)
    b_thr=$(echo "$b_config" | egrep -o "([0-9]+)" | head -n1)
    f_slp=$(echo "$f_config" | egrep -o "([0-9]+)" | tail -n1)
    b_slp=$(echo "$b_config" | egrep -o "([0-9]+)" | tail -n1)

    if [ $2 = "recv" ]; then
        echo "Most frames, best config: $f_thr threads, $f_slp us sleep: $fu_res"
        echo "Most bytes,  best config: $b_thr threads, $b_slp us sleep: $bu_res"
    else
        echo "Total goodput,  best config: $f_thr threads, $f_slp us sleep: $fu_res"
        echo "Single goodput, best config: $b_thr threads, $b_slp us sleep: $bu_res"
    fi
    rm /tmp/results_out
else
    echo "unknown role!"
fi

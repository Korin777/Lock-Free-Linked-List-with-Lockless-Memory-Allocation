#!/usr/bin/env bash

out_dir="out/data";
cores="all";
duration="2000";

[ -d "$out_dir" ] || mkdir -p $out_dir;

# settings

initials="128 1024 8192";
updates="25 50 100";

for initial in $initials; 
do
    echo "* -i$initial";

    range=$((2*$initial));

    for update in $updates;
    do
	echo "** -u$update";

        out="$out_dir/ll.i$initial.u$update.dat";
        scripts/scalability2.sh "$cores" \
            out/linklist out/linklist_MY_MALLOC \
            -d$duration -i$initial -r$range -u$update | tee $out;
    done
done


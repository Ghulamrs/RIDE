#!/bin/sh
# RIDE's Linux toolchain (~/ride/bin): x86 through gcc/as/ld, C6000 on vm6747.
set -u
BIN=$HOME/ride/bin; P=$HOME/rc/progs; W=$HOME/rc/w; rm -rf $W; mkdir -p $W; R=$HOME/rc/results.txt; : > $R; : > $R.detail
ls $BIN/lib | tr '\n' ' ' >> $R.detail; echo >> $R.detail
tool() { case $1 in c) echo cc1i;; cpp) echo cxx1i;; shl) echo shci;; esac; }
norm() { tr -d '\r' < "$1" | md5sum | cut -c1-8; }
while read name lang srcs; do
  exe=$(tool $lang); d=$W/$name; mkdir -p $d; files=""; for f in $srcs; do files="$files $P/$f"; done
  line="$name ($exe):"
  if (cd $d && timeout 120 $BIN/$exe.exe $files -o native > native.build 2>&1) && [ -f $d/native ]; then
     (cd $d && timeout 60 ./native < /dev/null > native.out 2>&1); e1=$?; o1=$(norm $d/native.out); line="$line x86-native=$o1/$e1"
  else o1=FAIL; line="$line x86-native=BUILD-FAIL"; echo "  $name native: $(tail -2 $d/native.build)" >> $R.detail; fi
  ss=""; ok=1; if [ $lang = shl ]; then targ="--target=tms6747"; else targ="-arch tms6747"; fi
  for f in $srcs; do case $f in *.h) continue;; esac; b=${f%.*}
    (cd $d && timeout 120 $BIN/$exe.exe -S $targ $P/$f -o c6-$b.s > c6-$b.build 2>&1) || { ok=0; echo "  $name c6 -S $f: $(tail -2 $d/c6-$b.build)" >> $R.detail; }
    ss="$ss c6-$b.s"; done
  rt=""; [ $lang = shl ] && rt="$BIN/lib/shmrt-tms6747"
  if [ $ok = 1 ]; then (cd $d && timeout 120 $BIN/vm6747.exe $ss $rt < /dev/null > c6.out 2>&1); e3=$?; o3=$(norm $d/c6.out); line="$line c6000-vm=$o3/$e3"
  else o3=FAIL; line="$line c6000-vm=S-FAIL"; fi
  agree=yes; [ "$o3" = "$o1" ] || agree=NO
  exp=""; [ -f $P/$name.expected ] && exp=" expected=$(norm $P/$name.expected)"
  echo "$line$exp agree=$agree" | tee -a $R
done < $P/list.txt
echo "=== details" | tee -a $R; cat $R.detail | tee -a $R; echo "=== done" | tee -a $R

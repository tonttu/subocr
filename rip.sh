#!/bin/bash

set -e

dvdbackup -Mp -o dvd.tmp
cd dvd.tmp/*
BASE=`pwd`
BASE=`basename "$BASE"`
cd VIDEO_TS

../../../gen-titles.rb > titles
PREFIX=`ls -rS|tail -n1|sed 's/_[0-9]*\.VOB$//'`
cat ${PREFIX}_*.VOB | tcextract -x ps1 -t vob -a 0x25 > subs.ps1
../../../subtitleripper/subtitle2pgm -i subs.ps1 -o subs -g 4 -C 0
../../../subocr/subocr *.png
../../../subocr/combine.rb 'test-%02d.srt' 0 0 0 0 0 1 < subs.srtx

mkdir -p "$BASE"
cp subs.ps1 "$BASE/"
rm -f "$BASE/subs.ps1.bz2"
bzip2 --best "$BASE/subs.ps1"
cp titles "$BASE/titles"

echo "Test \"`pwd`/test-00.srt\" with mplayer"
echo
echo "After that give timing parameters, for example: Foo.S01E04.srt -100 1 12 42000 40 55"

while true; do
  read -p "<basename> <delay1> <min1> <sec1> <delay2> <min2> <sec2>: " basename p0 p1 p2 p3 p4 p5
  if [ -n "$p5" ]; then
    break
  fi
done

../../../subocr/combine.rb "$BASE/$basename" $p0 $p1 $p2 $p3 $p4 $p5 < subs.srtx

mv "$BASE" ../../../
cd ../../..
ls -l "$BASE/"*

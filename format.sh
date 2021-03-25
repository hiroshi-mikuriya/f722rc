#!/bin/bash -eu
HERE=$(cd $(dirname $0); pwd)
TARGETS=(Core)

cd $HERE

for sample in "${TARGETS[@]}"
do
  files=`find ${sample} -type f -name *.c -or -name *.cpp -or -name *.h -or -name *.hpp`
  for f in $files
  do
    if [ ! "`echo $f | grep 'CMakeFiles'`" ]; then
      cmd="clang-format -i $f"
      echo $cmd
      `${cmd}`
    fi
  done
done


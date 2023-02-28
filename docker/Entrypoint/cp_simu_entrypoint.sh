#!/bin/bash

cp_args=""
diagnostic_file_size=0

for i in "$@"; do
  case $i in
    --diagnostic_file_size*)
      diagnostic_file_size="$2"
      shift
      ;;
    -*)
      cp_args+=" $i $2"
      shift
      ;;
    *)
      shift
      ;;
  esac
done

# create big file and add it in diagnostic zip of charge point
if [ $diagnostic_file_size -gt 0 ]
then
  echo "generate $diagnostic_file_size random strings in cp config file"
  echo "$(tr -dc A-Za-z0-9 < /dev/urandom | head -c $diagnostic_file_size)" >>  /var/chargepoint/blob.txt
  cp_args+=" -f blob.txt"
fi

# start simulator
echo "start charpoint with arguments : $cp_args"
/cp_simulator/chargepoint $cp_args

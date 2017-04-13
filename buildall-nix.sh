#!/usr/bin/env bash
threads=$1

if  ! [[ -n "$threads" ]]; then
  echo "Usage: ./buildall-nix.sh threadcount"
  echo
  echo "Usually, specify 1 more thread than you have CPU cores."
  echo "example with 8 CPU cores: ./buildall-nix.sh 9"
  exit
fi

./autogen.sh
./configure --with-incompatible-bdb
make -j $threads

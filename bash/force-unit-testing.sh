#!/bin/bash

##########################################################################
# 
# This file is part of FORCE - Framework for Operational Radiometric 
# Correction for Environmental monitoring.
# 
# Copyright (C) 2013-2022 David Frantz
# 
# FORCE is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# FORCE is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with FORCE.  If not, see <http://www.gnu.org/licenses/>.
# 
##########################################################################

# Run unit tests

# functions/definitions ------------------------------------------------------------------
PROG=$(basename "$0")
BIN="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
MISC="$BIN/force-misc"
TEST="$BIN/force-test"
export PROG BIN MISC TEST

# source bash "library" file
LIB="$MISC/force-bash-library.sh"
if ! eval ". ${LIB}" >/dev/null 2>&1; then 
  echo "loading bash library failed" 1>&2
  exit 1
fi

help(){
cat <<HELP

Usage: $PROG [-hvi]

  optional:
  -h = show this help
  -v = show version
  -i = show program's purpose

  -----
    see https://force-eo.readthedocs.io

HELP
exit 1
}
export -f help



# now get the options --------------------------------------------------------------------
ARGS=$(getopt -o hvi --long help,version,info -n "$0" -- "$@")
if [ $? != 0 ] ; then help; fi
eval set -- "$ARGS"

while :; do
  case "$1" in
    -h|--help) help ;;
    -v|--version) force_version; exit 0;;
    -i|--info) echo "Run unit tests (DevOps only, not meant for users)"; exit 0;;
    -- ) shift; break ;;
    * ) break ;;
  esac
  shift
done


# main thing -----------------------------------------------------------------------------

echo "##########################################################################"
echo ""
echo "Start of unit testing: $(date +"%Y-%m-%d %H:%M:%S")"
echo ""

units="$TEST/*"
debug "$units"

echo "$(echo $units | wc -w)" "tests were found."
echo ""
echo "##########################################################################"
echo ""

errors=0

for cmd in $units; do

  echo "Testing $cmd"
  if ! $cmd; then
    ((errors++))
  fi
  echo ""

done

echo ""
echo "##########################################################################"
echo ""
if [ "$errors" -eq 0 ]; then
  echo "all tests passed, all OK"
else 
  echoerr "$errors test(s) have failed, better get some coffee"
fi
echo ""
echo "End of unit testing: $(date +"%Y-%m-%d %H:%M:%S")"
echo ""
echo "##########################################################################"

if [ "$errors" -eq 0 ]; then
  exit 0
else
  exit 1
fi

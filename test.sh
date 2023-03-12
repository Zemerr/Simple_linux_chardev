#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color


DEFAUALT_DEVICE_NAME="custom_device"
DEVICE_NUM=2

DEFAULT_APP_NAME="device_app"


REFERENCE_STRING="123456"
test_arr1=("12345" "dog" "123456789" "1111111111111" "$REFERENCE_STRING" "22222222")

function write_string_dev() {
    local dev="0"
    sleep 4

    if [ $# -eq 0 ] ; then
        dev="0"
    else
        dev=$1
    fi

    echo ""
    echo "Start writing strings is file ..."
    echo ""

    for str in ${test_arr1[@]}; do
        echo $str > /dev/${DEFAUALT_DEVICE_NAME}${dev}
    done
}

function read_string_dev() {
    local dev= "0"

    if [ $# -eq 0 ] ; then
        dev="0"
    else
        dev=$1
    fi
    cat /dev/${DEFAUALT_DEVICE_NAME}${dev}
}

function read_strings_debugfs() {
    local dev="0"

    echo "Read debugfs device dir..."
    echo ""
    sleep 2

    if [ $# -eq 0 ] ; then
        dev="0"
    else
        dev=$1
    fi
    (set -x ;sudo cat /sys/kernel/debug/${DEFAUALT_DEVICE_NAME}/${dev})
    echo ""
}

function fasync_test() {
    local dev="0"

    if [ $# -eq 0 ] ; then
        dev="0"
    else
        dev=$1
    fi

    write_string_dev ${dev} &
    echo "Start Application - $DEFAULT_APP_NAME"
    echo ""
    ./$DEFAULT_APP_NAME -t -s $REFERENCE_STRING -f /dev/${DEFAUALT_DEVICE_NAME}${dev}
    result=$?
}


function run_simple_test() {
    local id=0

    echo "***********************  RUN TEST  **********************"
    echo ""
    echo ""
    while [ $id -lt $DEVICE_NUM ]
    do
        sleep 3
        echo "TEST DEVICE ----- /dev/${DEFAUALT_DEVICE_NAME}${id}"
        echo ""
        fasync_test $id
        read_strings_debugfs $id

        if [ $result -eq 2 ] ; then
            printf "${GREEN}FASYNC Test for /dev/${DEFAUALT_DEVICE_NAME}${id} PASSED${NC}\n"
        else
            printf "${RED}FASYNC Test for /dev/${DEFAUALT_DEVICE_NAME}${id} FAILED${NC}\n"
        fi

        id=`expr $id + 1`
        echo ""
        echo ""
        echo "------------------------------------------------------"
        echo""
        echo ""
    done

}

# run_simple_test


while getopts 'c:f:' opt; do
  case "$opt" in
    f)
      DEFAUALT_DEVICE_NAME="$OPTARG"
      ;;
    ?|h)
      echo "Usage: $(basename $0) [-f arg]"
      exit 1
      ;;
  esac
done
shift "$(($OPTIND -1))"



run_simple_test
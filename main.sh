#!/bin/bash


RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

DEVICE_NAME="custom_device"
MODULE_NAME="custom_device"
APP_NAME="device_app"

DRIVER_DIR=$(pwd)/driver
APP_DIR=$(pwd)/app
CURRENT_DIR=$(pwd)


function start_kernel_driver() {
    cd $DRIVER_DIR || exit -1

    if [ -e /dev/${DEVICE_NAME}0 ] || [ -e /dev/${DEVICE_NAME}0 ]; then
        echo "Device already exist, removing devices /dev/$DEVICE_NAME[0-2]"
        (set -x; sudo rm -f /dev/${DEVICE_NAME}[0-2])
        echo ""
    fi

    if [ -n "$(lsmod | grep -i $MODULE_NAME)" ]; then
        echo "Module already loaded, removing module $MODULE_NAME"
        make unload
        echo ""
    fi

    echo "1. Build module"
    make
    echo ""

    echo "2. Load module"
    make load
    echo ""

    echo "3. Create 2 devices"
    major=$(awk "\$2==\"$MODULE_NAME\" {print \$1}" /proc/devices)
    echo "major = $major"
    (set -x; sudo mknod /dev/${DEVICE_NAME}0 c $major 0)
    (set -x; sudo mknod /dev/${DEVICE_NAME}1 c $major 1)
    echo ""

    echo "4. Change devices owner"
    (set -x; sudo chown $(whoami) /dev/${DEVICE_NAME}0)
    (set -x; sudo chown $(whoami) /dev/${DEVICE_NAME}1)
    echo ""


    if [ -e "./${MODULE_NAME}.ko" ]; then
        printf "${GREEN}Module build successfully${NC}\n"
    else
        printf "${RED}Build error${NC}\n"
    fi

    if [ -n "$(lsmod | grep -i $MODULE_NAME)" ]; then
        printf "${GREEN}Module loaded successfully${NC}\n"
    else
        printf "${RED}Load error${NC}\n"
    fi

    if [ -e /dev/${DEVICE_NAME}0 ] && [ -e /dev/${DEVICE_NAME}0 ]; then
        printf "${GREEN}Devices created successfully${NC}\n"
    else
        printf "${RED}Failed to create devices${NC}\n"
    fi
    echo ""
}

function stop_kernel_driver() {
    cd $DRIVER_DIR || exit -1

    if [ -e /dev/${DEVICE_NAME}0 ] && [ -e /dev/${DEVICE_NAME}0 ]; then
        echo "Device exist, removing devices /dev/$DEVICE_NAME[0-2]"
        (set -x; sudo rm -f /dev/${DEVICE_NAME}[0-2])
        echo ""
    fi

    if [ -n "$(lsmod | grep -i $MODULE_NAME)" ]; then
        echo "Module loaded, removing module $MODULE_NAME"
        make unload
        echo ""
    fi
    make clean

}


function start_app() {
    echo "Build application"
    echo ""
    cd $APP_DIR || exit -1
    make
    (set -x; cp $APP_NAME ./..)

    if [ -e $APP_NAME ]; then
        printf "${GREEN}App build successfully${NC}\n\n\n"
    else
        printf "${RED}Failed to build app${NC}\n\n\n"
    fi
}

function stop_app() {
    cd $APP_DIR || exit -1
    make clean
    (set -x; rm -f ../$APP_NAME)
}





case "$1" in
  start)
    start_kernel_driver
    start_app
    ;;
  stop)
    stop_kernel_driver
    stop_app
    ;;
  test)
    start_kernel_driver
    start_app
    ${CURRENT_DIR}/test.sh -f $DEVICE_NAME
    ;;
  *)
    echo "Usage: main {start|stop|test}" >&2
    ;;
esac

exit 0

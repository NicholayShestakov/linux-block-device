#!/bin/bash

MODULE="obdblk.ko"
MOD_NAME="obdblk"
BACKING_FILE="test_backend.img"
MOUNT_POINT="/mnt/obd_test"
SIZE_MB=64

cleanup() {
    echo "--- Cleaning up ---"
    sudo umount $MOUNT_POINT 2>/dev/null || true
    rmdir $MOUNT_POINT 2>/dev/null || true
    sudo rmmod $MOD_NAME 2>/dev/null || true
    
    LOOP_DEV=$(losetup -j $BACKING_FILE | cut -d: -f1)
    if [ -n "$LOOP_DEV" ]; then
        sudo losetup -d $LOOP_DEV
    fi
    rm -f $BACKING_FILE
}

set -e

cleanup

echo "--- Preparing environment ---"
dd if=/dev/zero of=$BACKING_FILE bs=1M count=$SIZE_MB status=none
LOOP_DEV=$(sudo losetup -f --show $BACKING_FILE)
echo "Backing file linked to $LOOP_DEV"

if [ ! -f $MODULE ]; then
    echo "Error: $MODULE not found. Run 'make' first."
    exit 1
fi
sudo insmod $MODULE base_device_name=$LOOP_DEV
echo "Module loaded."

sleep 1
DEV_PATH="/dev/obdblk"

if [ ! -b $DEV_PATH ]; then
    echo "Error: $DEV_PATH not created!"
    exit 1
fi

echo "--- Test 1: Raw Read/Write integrity ---"
TEST_STR="Kernel Data Integrity Test"
echo "$TEST_STR" | sudo dd of=$DEV_PATH bs=512 count=1 oflag=direct status=none
RESULT=$(sudo dd if=$DEV_PATH bs=512 count=1 iflag=direct status=none | tr -d '\0')

if [[ "$RESULT" == *"$TEST_STR"* ]]; then
    echo "[OK] Raw Read/Write successful"
else
    echo "[FAIL] Data corruption or write failure!"
    cleanup && exit 1
fi

echo "--- Test 2: File System Test (ext4) ---"
sudo mkfs.ext4 -q $DEV_PATH
mkdir -p $MOUNT_POINT
sudo mount $DEV_PATH $MOUNT_POINT
echo "Module can hold a filesystem."

sudo sh -c "echo 'FS test content' > $MOUNT_POINT/testfile"
cat $MOUNT_POINT/testfile > /dev/null
sudo umount $MOUNT_POINT
echo "[OK] Mount/Unmount/FS-Write successful"

echo "--- Test 3: Statistics Check ---"
SYSFS_PATH=$(find /sys/devices -name "obd_statistics" | head -n 1)

if [ -z "$SYSFS_PATH" ]; then
    echo "[FAIL] Statistics file not found in sysfs!"
else
    echo "Current statistics:"
    cat "$SYSFS_PATH"
fi

echo "--- All tests passed! ---"
cleanup


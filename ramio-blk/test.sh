#!/bin/bash

MODULE="ramioblk.ko"
MOD_NAME="ramioblk"
DEV_PATH="/dev/ramioblk"
MOUNT_POINT="/mnt/ramio_test"
TEST_FILE_SIZE_MB=10

cleanup() {
    echo "--- Cleaning up ---"
    sudo umount $MOUNT_POINT 2>/dev/null || true
    rmdir $MOUNT_POINT 2>/dev/null || true
    sudo rmmod $MOD_NAME 2>/dev/null || true
    rm -f /tmp/ramio_src /tmp/ramio_dst
}

set -e

cleanup

echo "--- Preparing environment ---"
if [ ! -f $MODULE ]; then
    echo "Error: $MODULE not found. Run 'make' first."
    exit 1
fi

sudo insmod $MODULE
echo "Module $MOD_NAME loaded successfully."

sleep 1

if [ ! -b $DEV_PATH ]; then
    echo "Error: $DEV_PATH not created!"
    exit 1
fi

echo "--- Test 1: Raw I/O Integrity ---"
TEST_STR="Ramioblk Data Integrity Check $(date)"
echo "$TEST_STR" | sudo dd of=$DEV_PATH bs=512 count=1 oflag=direct status=none
RESULT=$(sudo dd if=$DEV_PATH bs=512 count=1 iflag=direct status=none | tr -d '\0')

if [[ "$RESULT" == *"$TEST_STR"* ]]; then
    echo "[OK] Raw Read/Write successful"
else
    echo "[FAIL] Data corruption detected!"
    cleanup && exit 1
fi

echo "--- Test 2: Bulk Transfer (MD5 Verification) ---"
dd if=/dev/urandom of=/tmp/ramio_src bs=1M count=$TEST_FILE_SIZE_MB status=none
sudo dd if=/tmp/ramio_src of=$DEV_PATH bs=1M count=$TEST_FILE_SIZE_MB oflag=direct status=none
sudo dd if=$DEV_PATH of=/tmp/ramio_dst bs=1M count=$TEST_FILE_SIZE_MB iflag=direct status=none

SRC_HASH=$(md5sum /tmp/ramio_src | awk '{print $1}')
DST_HASH=$(md5sum /tmp/ramio_dst | awk '{print $1}')

if [ "$SRC_HASH" == "$DST_HASH" ]; then
    echo "[OK] Hash sum match for $TEST_FILE_SIZE_MB MB transfer"
else
    echo "[FAIL] Hash mismatch! Block mapping might be broken."
    cleanup && exit 1
fi

echo "--- Test 3: Filesystem and blk-mq Stress ---"
sudo mkfs.ext4 -q $DEV_PATH
mkdir -p $MOUNT_POINT
sudo mount $DEV_PATH $MOUNT_POINT
echo "Module handles ext4 filesystem successfully."

sudo sh -c "echo 'Multisegment BIO test' > $MOUNT_POINT/testfile"
if grep -q "Multisegment" $MOUNT_POINT/testfile; then
    echo "[OK] Filesystem Write/Read successful"
else
    echo "[FAIL] Filesystem I/O error"
    cleanup && exit 1
fi

echo "--- All tests passed! ---"
cleanup


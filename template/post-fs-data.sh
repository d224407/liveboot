#!/system/bin/sh

MODDIR=${0%/*}

# Chạy liveboot ngay khi dữ liệu được mount
"$MODDIR/system/bin/liveboot" "$MODDIR/config" > /dev/null 2>&1 &

echo "LiveBoot started at $(date)" > /data/local/tmp/liveboot_postfs.log
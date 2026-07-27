#!/system/bin/sh

# Kill any running liveboot processes
pkill -f "liveboot" 2>/dev/null

# Clean up logs
rm -f /data/local/tmp/liveboot_postfs.log
rm -f /data/local/tmp/liveboot_service.log
rm -f /data/local/tmp/liveboot_error.log

echo "LiveBoot uninstalled at $(date)" > /data/local/tmp/liveboot_uninstall.log
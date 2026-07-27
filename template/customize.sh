#!/system/bin/sh

SKIPUNZIP=0

ui_print() { echo "$1"; }

ui_print "- Getting screen size"

output="$(wm size 2>/dev/null)"
if [ "$output" ]; then
    width_height="$(echo $output | tr -s ' ' ':' | cut -d':' -f3)"
    width="$(echo $width_height | cut -d'x' -f1)"
    height="$(echo $width_height | cut -d'x' -f2)"
else
    width=1080
    height=1920
fi

ui_print "  - $width x $height"

ui_print "- Generating config file"

cat > "$MODPATH/config" <<EOF
dark
logcatlevels=WEFS
logcatbuffers=C
logcatformat=brief
logcatnocolors
dmesg=0--1
lines=80
wordwrap
fallbackwidth=$width
fallbackheight=$height
EOF

ui_print "- Detecting architecture"

ARCH=$(getprop ro.product.cpu.abi)
case "$ARCH" in
    arm64-v8a|arm64)
        BIN_SRC="liveboot_64"
        ui_print "  - Architecture: ARM64 (64-bit)"
        ;;
    armeabi-v7a|armeabi)
        BIN_SRC="liveboot_32"
        ui_print "  - Architecture: ARMv7 (32-bit)"
        ;;
    x86_64)
        BIN_SRC="liveboot_x64"
        ui_print "  - Architecture: x86_64 (64-bit)"
        ;;
    x86|i686|i586|i486|i386)
        BIN_SRC="liveboot_x86"
        ui_print "  - Architecture: x86 (32-bit)"
        ;;
    *)
        if [ -f "$MODPATH/system/bin/liveboot_64" ]; then
            BIN_SRC="liveboot_64"
            ui_print "  - Architecture: ARM64 (fallback)"
        elif [ -f "$MODPATH/system/bin/liveboot_32" ]; then
            BIN_SRC="liveboot_32"
            ui_print "  - Architecture: ARMv7 (fallback)"
        else
            ui_print "  - ! No compatible binary found!"
            abort
        fi
        ;;
esac

ui_print "- Installing binary"

cp "$MODPATH/system/bin/$BIN_SRC" "$MODPATH/system/bin/liveboot"
chmod 755 "$MODPATH/system/bin/liveboot"

ui_print "  - Binary size: $(ls -lh "$MODPATH/system/bin/liveboot" | awk '{print $5}')"

ui_print "- Removing unused binaries"

rm -f "$MODPATH/system/bin/liveboot_32" \
      "$MODPATH/system/bin/liveboot_64" \
      "$MODPATH/system/bin/liveboot_x86" \
      "$MODPATH/system/bin/liveboot_x64"

ui_print "- Setting file permissions"

set_perm "$MODPATH/system/bin/liveboot" 0 0 0755
set_perm "$MODPATH/config" 0 0 0644
set_perm "$MODPATH/service.sh" 0 0 0755
set_perm "$MODPATH/post-fs-data.sh" 0 0 0755
set_perm "$MODPATH/action.sh" 0 0 0755
set_perm "$MODPATH/uninstall.sh" 0 0 0755

ui_print ""
ui_print "======================================"
ui_print "  ✅ LiveBoot installed successfully!"
ui_print "  Logs will be displayed on boot"
ui_print "  Config: $MODPATH/config"
ui_print "  Architecture: $ARCH"
ui_print "======================================"
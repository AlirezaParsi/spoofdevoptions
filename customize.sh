#!/system/bin/sh

# Module installation script
MODDIR=${0%/*}

# Ensure module directory permissions
chmod -R 755 $MODDIR/zygisk

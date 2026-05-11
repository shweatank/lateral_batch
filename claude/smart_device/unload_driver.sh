#!/bin/bash

MODULE="smart_driver"

echo "Unloading module..."
sudo rmmod ${MODULE}

if [ $? -eq 0 ]; then
    echo "Module unloaded successfully."
    echo "Kernel logs:"
    sudo dmesg | tail -n 5
    
    make clean > /dev/null
    echo "Cleaned up build files."
else
    echo "Failed to unload module. Is it in use?"
fi

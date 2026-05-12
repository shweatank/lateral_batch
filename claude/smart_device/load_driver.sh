#!/bin/bash

MODULE="smart_driver"
DEVICE="smart_device"

echo "Building module..."
make

echo "Loading module..."
sudo insmod ${MODULE}.ko

if [ $? -eq 0 ]; then
    echo "Module loaded successfully."
    
    # Check if udev created the node automatically, if not, wait or create
    if [ ! -c "/dev/${DEVICE}" ]; then
        echo "Waiting for udev to create device node..."
        sleep 1
    fi
    
    # Change permissions so standard user can run user_app without sudo
    if [ -c "/dev/${DEVICE}" ]; then
        sudo chmod 666 /dev/${DEVICE}
        echo "Permissions set to 666 for /dev/${DEVICE}"
    else
        echo "WARNING: Device node /dev/${DEVICE} not found!"
    fi

    echo "Kernel logs:"
    sudo dmesg | tail -n 5
else
    echo "Failed to load module."
fi

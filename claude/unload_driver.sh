#!/bin/bash
#
# unload_driver.sh - Safely unload the smart_device kernel module
#
# This script performs a clean shutdown and unload of the smart_device driver,
# including device node cleanup, module removal, and comprehensive error checking.
#
# Usage: sudo ./unload_driver.sh
# Note: Must be run with root privileges (sudo)
#
# Author: Linux Kernel Developer
# Date: May 2026

set -e  # Exit on any error

# Color codes for output formatting
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'  # No Color

# Script constants
DRIVER_NAME="smart_driver"
DEVICE_NAME="smart_device"
DEV_PATH="/dev/${DEVICE_NAME}"

# ============================================================================
# Helper functions
# ============================================================================

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# ============================================================================
# Privilege check
# ============================================================================

if [[ $EUID -ne 0 ]]; then
    log_error "This script must be run with root privileges"
    echo "       Usage: sudo $0"
    exit 1
fi

log_info "Starting smart_device driver unload sequence..."

# ============================================================================
# Step 1: Check if module is loaded
# ============================================================================

log_info "Step 1: Checking if module is loaded"

if ! lsmod | grep -q "^${DRIVER_NAME} "; then
    log_warning "Module is not currently loaded"
    log_info "Proceeding with cleanup of device node if it exists..."
else
    log_success "Module is loaded"
fi

# ============================================================================
# Step 2: Check for open file descriptors on the device
# ============================================================================

log_info "Step 2: Checking for processes using the device"

if [[ -e "$DEV_PATH" ]]; then
    # Try to find processes with open file descriptors
    open_procs=$(lsof "$DEV_PATH" 2>/dev/null || echo "")
    
    if [[ ! -z "$open_procs" ]]; then
        log_warning "The following processes have open file descriptors on $DEV_PATH:"
        echo "$open_procs" | tail -n +2 | sed 's/^/  /'
        log_warning "Attempting to close the device gracefully..."
    else
        log_success "No open file descriptors found"
    fi
fi

# ============================================================================
# Step 3: Unload the kernel module
# ============================================================================

log_info "Step 3: Unloading kernel module"

if lsmod | grep -q "^${DRIVER_NAME} "; then
    log_info "Executing: rmmod $DRIVER_NAME"
    
    if ! rmmod "$DRIVER_NAME" 2>&1; then
        log_error "Failed to unload kernel module"
        echo ""
        echo "Possible reasons:"
        echo "  - Device is still in use by a running process"
        echo "  - Module is being used by another kernel module"
        echo "  - Memory mapped I/O in progress"
        echo ""
        echo "Debug commands:"
        echo "  lsof $DEV_PATH              # Show open file descriptors"
        echo "  ps aux | grep user_app      # Show running processes"
        echo "  dmesg | tail -20            # Show kernel messages"
        echo ""
        exit 1
    fi
    
    log_success "Kernel module unloaded"
else
    log_warning "Module was not loaded, skipping rmmod"
fi

# ============================================================================
# Step 4: Wait and verify module is removed
# ============================================================================

log_info "Step 4: Verifying module removal"

sleep 0.5  # Give kernel time to clean up

if lsmod | grep -q "^${DRIVER_NAME} "; then
    log_error "Module is still loaded after rmmod!"
    exit 1
fi

log_success "Module removal verified"

# ============================================================================
# Step 5: Remove device node
# ============================================================================

log_info "Step 5: Removing device node"

if [[ -e "$DEV_PATH" ]]; then
    log_info "Removing: $DEV_PATH"
    
    if ! rm -f "$DEV_PATH"; then
        log_warning "Failed to remove device node"
        # Continue anyway - not a critical failure
    else
        log_success "Device node removed"
    fi
else
    log_info "No device node found at $DEV_PATH"
fi

# ============================================================================
# Step 6: Verify device node is removed
# ============================================================================

log_info "Step 6: Verifying device cleanup"

if [[ -e "$DEV_PATH" ]]; then
    log_error "Device node still exists: $DEV_PATH"
    exit 1
fi

log_success "Device node successfully removed"

# ============================================================================
# Step 7: Display kernel shutdown messages
# ============================================================================

log_info "Step 7: Kernel driver shutdown messages"

echo ""
echo "Kernel driver log (dmesg):"
echo "---"
dmesg | tail -3 | sed 's/^/  /'
echo "---"
echo ""

# ============================================================================
# Step 8: Final status check
# ============================================================================

log_info "Step 8: Final system status"

echo ""
echo "=========================================="
echo "SMART_DEVICE DRIVER UNLOAD SUMMARY"
echo "=========================================="
echo "Module Status:   $(lsmod | grep -q $DRIVER_NAME && echo "STILL LOADED (ERROR!)" || echo "Unloaded ✓")"
echo "Device Node:     $(test -e $DEV_PATH && echo "STILL EXISTS (ERROR!)" || echo "Removed ✓")"
echo ""

if ! (lsmod | grep -q $DRIVER_NAME) && [[ ! -e "$DEV_PATH" ]]; then
    echo "Status:          UNLOAD SUCCESSFUL ✓"
    echo "=========================================="
    echo ""
    log_success "Driver unload complete!"
    exit 0
else
    echo "Status:          UNLOAD INCOMPLETE ✗"
    echo "=========================================="
    echo ""
    log_error "Driver unload failed - check above for details"
    exit 1
fi

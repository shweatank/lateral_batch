#!/bin/bash
#
# load_driver.sh - Load the smart_device kernel module and verify setup
#
# This script automates the kernel module loading process, device node creation,
# verification, and permission setup. It includes comprehensive error checking
# and informative output for debugging and validation.
#
# Usage: sudo ./load_driver.sh
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
MODULE_PATH="./smart_device/smart_driver.ko"
DRIVER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE_FULL_PATH="${DRIVER_DIR}/smart_device/${DRIVER_NAME}.ko"
DEV_PATH="/dev/${DEVICE_NAME}"

# ============================================================================
# Helper functions for logging and formatting
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

log_info "Starting smart_device driver load sequence..."

# ============================================================================
# Step 1: Verify that the kernel module exists
# ============================================================================

log_info "Step 1: Verifying kernel module existence"

if [[ ! -f "$MODULE_FULL_PATH" ]]; then
    log_error "Module file not found: $MODULE_FULL_PATH"
    echo ""
    echo "You must first compile the driver:"
    echo "  $ cd ${DRIVER_DIR}/smart_device"
    echo "  $ make"
    echo ""
    exit 1
fi

log_success "Module file found: $MODULE_FULL_PATH"
stat_output=$(stat -c "Size: %s bytes, Modified: %y" "$MODULE_FULL_PATH")
log_info "  Module details: $stat_output"

# ============================================================================
# Step 2: Check if the module is already loaded
# ============================================================================

log_info "Step 2: Checking for existing module instances"

if lsmod | grep -q "^${DRIVER_NAME} "; then
    log_warning "Driver module is already loaded"
    log_info "Attempting to unload the existing instance first..."
    
    if ! rmmod "$DRIVER_NAME" 2>/dev/null; then
        log_error "Failed to unload existing module"
        exit 1
    fi
    
    log_success "Existing module unloaded"
fi

# ============================================================================
# Step 3: Remove any existing device node
# ============================================================================

log_info "Step 3: Cleaning up any existing device node"

if [[ -e "$DEV_PATH" ]]; then
    log_warning "Device node already exists: $DEV_PATH"
    rm -f "$DEV_PATH"
    log_success "Device node removed"
else
    log_info "No existing device node found"
fi

# ============================================================================
# Step 4: Load the kernel module
# ============================================================================

log_info "Step 4: Loading kernel module"
log_info "Executing: insmod $MODULE_FULL_PATH"

if ! insmod "$MODULE_FULL_PATH"; then
    log_error "Failed to load kernel module"
    echo ""
    echo "Debug information:"
    echo "  Check kernel logs: dmesg | tail -20"
    echo ""
    exit 1
fi

log_success "Kernel module loaded successfully"

# ============================================================================
# Step 5: Wait for device node and verify automatic creation
# ============================================================================

log_info "Step 5: Waiting for automatic device node creation (udev)"

# Wait up to 5 seconds for udev to create the device node
counter=0
max_attempts=50  # 50 * 0.1s = 5 seconds

while [[ ! -e "$DEV_PATH" && $counter -lt $max_attempts ]]; do
    sleep 0.1
    ((counter++))
done

if [[ ! -e "$DEV_PATH" ]]; then
    log_error "Device node was not created by udev"
    log_warning "Attempting manual device node creation..."
    
    # Get the major and minor numbers from the loaded module
    major=$(grep smart_device /proc/devices | awk '{print $1}')
    
    if [[ -z "$major" ]]; then
        log_error "Could not determine major device number"
        log_info "Unloading module..."
        rmmod "$DRIVER_NAME"
        exit 1
    fi
    
    log_info "Creating device node manually: mknod $DEV_PATH c $major 0"
    mknod "$DEV_PATH" c "$major" 0
fi

log_success "Device node created: $DEV_PATH"

# ============================================================================
# Step 6: Verify device node permissions and fix if necessary
# ============================================================================

log_info "Step 6: Setting device node permissions"

# Get current permissions
current_perms=$(stat -c "%A" "$DEV_PATH")
log_info "Current permissions: $current_perms"

# Make the device readable and writable by the user
chmod 666 "$DEV_PATH"

new_perms=$(stat -c "%A" "$DEV_PATH")
log_success "Device permissions updated to: $new_perms"

# ============================================================================
# Step 7: Verify module is loaded and collect module information
# ============================================================================

log_info "Step 7: Verifying module load and collecting information"

if ! lsmod | grep -q "^${DRIVER_NAME} "; then
    log_error "Module verification failed - module not in lsmod output"
    exit 1
fi

log_success "Module verified in kernel"

# Extract module info
module_info=$(lsmod | grep "^${DRIVER_NAME}")
log_info "Module info: $module_info"

# Get device major and minor numbers
major=$(grep smart_device /proc/devices | awk '{print $1}')
minor=$(stat -c "%t" "$DEV_PATH" | tr '[:lower:]' '[:upper:]')
major_from_dev=$(stat -c "%T" "$DEV_PATH" | tr '[:lower:]' '[:upper:]')

log_info "Device major number: $major"
log_info "Device minor number: $minor"

# ============================================================================
# Step 8: Display kernel log output
# ============================================================================

log_info "Step 8: Kernel driver initialization messages"

echo ""
echo "Kernel driver log (dmesg):"
echo "---"
dmesg | tail -5 | sed 's/^/  /'
echo "---"
echo ""

# ============================================================================
# Step 9: Final verification - test basic device operations
# ============================================================================

log_info "Step 9: Performing basic device verification"

# Try to open and close the device
if ! exec 3<>"$DEV_PATH" 2>/dev/null; then
    log_error "Failed to open device node for testing"
    exit 1
fi

log_success "Device node opened successfully"

# Test write operation
test_data="hello smart_device"
if ! echo "$test_data" >&3 2>/dev/null; then
    log_warning "Test write failed (may be expected)"
else
    log_success "Test write operation successful"
fi

# Close the device
exec 3>&-

log_success "Basic device operations verified"

# ============================================================================
# Step 10: Display summary information
# ============================================================================

log_info "Step 10: Load operation completed successfully!"

echo ""
echo "=========================================="
echo "SMART_DEVICE DRIVER LOAD SUMMARY"
echo "=========================================="
echo "Device Name:     $DEVICE_NAME"
echo "Device Path:     $DEV_PATH"
echo "Major Number:    $major"
echo "Module Status:   $(lsmod | grep $DRIVER_NAME | awk '{print "Loaded (" $3 " bytes)"}' || echo "ERROR")"
echo "Device Status:   $(test -e $DEV_PATH && echo "Created" || echo "ERROR")"
echo "Permissions:     $(stat -c "%A" $DEV_PATH)"
echo ""
echo "Next steps:"
echo "  1. Verify with:  lsmod | grep $DRIVER_NAME"
echo "  2. Check logs:   dmesg | tail -20"
echo "  3. Test device:  ./smart_device/user_app"
echo "  4. Unload with:  sudo ./unload_driver.sh"
echo "=========================================="
echo ""

log_success "Driver load complete!"
exit 0

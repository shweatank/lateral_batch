savedcmd_/tmp/smart_device_build/smart_driver.mod := printf '%s\n'   smart_driver.o | awk '!x[$$0]++ { print("/tmp/smart_device_build/"$$0) }' > /tmp/smart_device_build/smart_driver.mod

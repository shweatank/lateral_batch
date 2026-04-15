savedcmd_workq.mod := printf '%s\n'   workq.o | awk '!x[$$0]++ { print("./"$$0) }' > workq.mod

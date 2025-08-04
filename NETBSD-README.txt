To use the L1VM binaries on NetBSD do the following memory size changes as root:

# ulimit -s 16384
# ulimit -d 1000000

Then open a new shell and now you can run the L1VM binaries without memory errors!

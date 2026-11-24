#!/usr/bin/env bash
# Compile and run the two SSL socket tests against a loopback TLS server.
#
# The server blocks in accept and the client blocks in connect, so they run in
# separate VM processes.  The self-signed certificate is staged under the VM
# sandbox root because the network module resolves file names there.

set -u

here="$(cd "$(dirname "$0")" && pwd)"
compiler="$here/../lisp2l1vm.lisp"
clispbin="${CLISP:-clisp}"
vmbuild="${L1VM_BUILD:-l1vm-build.sh}"
vm="${L1VM:-/home/stefan/develop/l1vm/vm/l1vm}"
modules="${L1VM_MODULES:-/home/stefan/develop/l1vm/modules}"
sandbox="${L1VM_SANDBOX:-$HOME/l1vm}"
ssl_dir="$here/ssl"

cert_src="$here/../tests/ssl/ssl-cert.pem"
cert_name="lisp-ssl-test.pem"
cert_dst="$sandbox/$cert_name"

if [ ! -f "$compiler" ]; then
    printf 'compiler not found: %s\n' "$compiler" >&2
    exit 2
fi
if [ ! -x "$vm" ]; then
    printf 'L1VM binary not found: %s (set L1VM to override)\n' "$vm" >&2
    exit 2
fi
if [ ! -f "$cert_src" ]; then
    printf 'certificate not found: %s\n' "$cert_src" >&2
    exit 2
fi

mkdir -p "$sandbox" || exit 2
cp "$cert_src" "$cert_dst" || exit 2
trap 'rm -f "$cert_dst"' EXIT

cd "$ssl_dir" || exit 2
for name in server client; do
    printf 'compiling ssl/%s.lisp ... ' "$name"
    if "$clispbin" "$compiler" "$name.lisp" "$name.l1com" >"$name.compile.log" 2>&1; then
        printf 'ok\n'
    else
        printf 'FAILED (see ssl/%s.compile.log)\n' "$name"
        exit 2
    fi
    if ! "$vmbuild" "$name" >"$name.build.log" 2>&1; then
        printf 'l1vm-build.sh FAILED (see ssl/%s.build.log)\n' "$name" >&2
        exit 2
    fi
done

rm -f server.log client.log
ld_path="$modules${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

( timeout 30s env LD_LIBRARY_PATH="$ld_path" "$vm" server >server.log 2>&1 ) &
server_pid=$!
sleep 1
timeout 30s env LD_LIBRARY_PATH="$ld_path" "$vm" client >client.log 2>&1
client_rc=$?
wait "$server_pid"
server_rc=$?

cat server.log
cat client.log

fail=0
if [ "$client_rc" -ne 0 ] || [ "$server_rc" -ne 0 ]; then
    printf 'SSL test processes exited with %s/%s\n' "$server_rc" "$client_rc" >&2
    fail=1
fi
for log in server.log client.log; do
    if grep -Eq 'FAIL|ERROR|FATAL|memory_bounds|epos|stpopi' "$log"; then
        printf 'SSL test diagnostics in ssl/%s\n' "$log" >&2
        fail=1
    fi
    if ! grep -q 'done' "$log"; then
        printf 'SSL test did not finish in ssl/%s\n' "$log" >&2
        fail=1
    fi
done
exit "$fail"

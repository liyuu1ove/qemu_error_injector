### QEMU setup

We can use docker image from cs162 @ https://cs162.org/static/hw/hw-intro/docs/setup/docker-workspace/ and that will give us a consistent develop environment.

### test QEMU x86-64

First, try to do a User-mode Emulation (Running a Single Binary). This mode is less common for x86-on-x86 simulation but is extremely useful for running a binary from a different architecture (e.g., running an ARM program on your x86 PC). It simulates the CPU instructions without simulating the whole system.

#### how to run it

1. Compile hello.c statically

```shell
gcc -static -o hello hello.c
```

2. Execute it with QEMU's user-mode emulator

```shell
qemu-x86_64 ./hello
```

3. Output

```
Hello from a QEMU-run binary!
```
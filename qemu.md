### Docker workspace setup

Use the `Dockerfile` in the repo to setup. I have attached all nessesary tools inside.

Usage:
```bash
sudo docker build -t qemu-build-env .
# mount this git repo to a new container
sudo docker run -it --name my-qemu-builder \
  -v "$(pwd)":/home/user1/code \
  qemu-build-env
```
Now you connect to a bash terminal inside the docker workspace.


```bash
cd code
```
Now you should see the `qemu_error_injector` git repo

Start the docker and exit

```bash
sudo docker start -ai my-qemu-builder
```

```bash
#inside the docker workspace terminal
exit
```

### QEMU build setup
1. download QEMU source code
```bash
#redirect to the code workspace
git clone https://gitlab.com/qemu-project/qemu.git
```
2. build the QEMU
It is a good practice to build it in the `build` directory.

```bash
cd qemu
mkdir build
cd build

# Configure to build ONLY the x86_64 and ARM 64-bit emulators.
../configure --target-list=x86_64-softmmu,x86_64-linux-user
# compile
make -j$(nproc)
```
### test QEMU x86-64

First, try to do a User-mode Emulation (Running a Single Binary). This mode is less common for x86-on-x86 simulation but is extremely useful for running a binary from a different architecture (e.g., running an ARM program on your x86 PC). It simulates the CPU instructions without simulating the whole system.
#### add qemu to PATH in docker
```
echo 'export PATH="/home/user1/code/qemu/build:$PATH"' >> ~/.bashrc
source ~/.bashrc
# now it should have output anywhere
qemu-system-x86_64 --version
```
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
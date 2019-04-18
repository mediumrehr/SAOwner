# SAOwner
SAO interposer &mdash; monitor, intercept, and block i2c communication over a badge's SAO port.

## Setup

#### arm gcc and gdb

MacOS

```
brew install armmbed/formulae/arm-none-eabi-gcc
```

#### jlink gdb server

download install file from [segger.com](https://www.segger.com/products/debug-probes/j-link/tools/j-link-gdb-server/about-j-link-gdb-server/)

## Build

```
cd SAOwner/
make -C Build/
```

## Load

#### jlink

Start jlink gdb server in one tab

```
JLinkGDBServer -if SWD -device ATSAMD21G18
```

Start gdb in another tba

```
arm-none-eabi-gdb Build/SAOwner.elf
(gdb) target extended-remote :2331
```

Load new firmware, reset microcontroller, and run new code

```
(gdb) load
(gdb) monitor reset
(gdb) c
```

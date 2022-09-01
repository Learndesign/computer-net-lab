#!/bin/bash

dd if=/dev/urandom bs=1MB count=1 | base64 > 1M.dat
dd if=/dev/urandom bs=1MB count=8 | base64 > 8M.dat
dd if=/dev/urandom bs=1MB count=32 | base64 > 32M.dat
dd if=/dev/urandom bs=1MB count=64 | base64 > 64M.dat

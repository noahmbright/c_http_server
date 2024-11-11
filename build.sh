#! /bin/sh

cmake -S . -B build
make -C build

chmod 000 files_to_serve/a_forbidden_file.html

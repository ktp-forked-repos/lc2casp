# lc2casp
A translator for logic programs with constraint atoms to CASP.

## Usage
The basic usage to ground, translate, and solve logic programs with constraint atoms is
```bash
gringo PROGRAM | lc2casp | clingcon
```

## Installation
The translator requires that gringo and clingo as well as clingcon are build first.

To build gringo and clingo, execute
```shell
cd third_party/gringo
scons --build=release
```
In case more configuration is necessary, please check the README and INSTALL documents of the gringo project.
The `gringo` executable that can be used to ground the examples is available in `third_party/gringo/build/release/gringo`.

To build clingcon, execute
```shell
cd third_party/clingcon
make
```
The `clingcon` executable that can be used to solve the examples is available in `third_party/clingcon/build/bin/clingcon`.

If both projects have been compiled successfully, the translator can be build
```shell
make
```
The translator executable `lc2casp` to rewrite the gringo output is available in the top level directory afterward.

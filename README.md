# DLS9-enumerator - Optimized MPI version of the DLS enumeration application

This repository contains **DLS9-enumerator**, the Intel® MPI optimized application for enumerating
[Diagonal Latin Squares (DLS)](https://en.wikipedia.org/wiki/Latin_square) of order 9.

On this moment, supports execution under Microsoft Windows 7 (or newer) on *x86-64* CPU and *Intel® Xeon Phi™ Coprocessors*.

*Tested* on Microsoft Windows 10 Pro and Microsoft Windows Server 2012 R2 systems with *[Intel® Xeon Phi™ x100 (KNC) Coprocessors](http://ark.intel.com/products/family/92649/Intel-Xeon-Phi-Coprocessor-x100-Product-Family)*.

## Requirements

This project optimized to build under Windows using Microsoft Visual Studio 2015 and [Intel® Parallel Studio XE 2016](https://software.intel.com/en-us/intel-parallel-studio-xe).

*Cross-compilation* for Intel® Xeon Phi™ Coprocessors under Microsoft Windows is supported as well.

## Usage

Open Microsoft Visual Studio solution *[dls9.sln](dls9.sln)* to build Microsoft Windows executable.
Use included Windows CMD batch file *[!cross.cmd](dls9/!cross.cmd)* to build version for Intel® Xeon Phi™ Coprocessor.

To run benchmarks on included [dls9_count_data](dls9/dls9_count_data) file:

System           | Command
-----------------|-----------
Windows PC       | *[!bench.cmd](dls9/!bench.cmd)* *\[min-ranks\]* *\[max-ranks\]*
Intel® Xeon Phi™ | *[bench](dls9/bench)* *\[min-ranks\]* *\[max-ranks\]* (on Coprocessor)

## Contributing

**[DLS9-enumerator](https://github.com/assa77/DLS9-enumerator)** was developed by *[Alexander M. Albertian](mailto:assa@4ip.ru)*.
DLS enumeration is based on *[Nauchnik (Oleg Zaikin)](https://github.com/Nauchnik)* [sources](https://github.com/Nauchnik/DLS9_enumeration).

All contributions are welcome! Please do *NOT* use an editor that automatically reformats whitespace.

## Download

You can download precompiled project executables from [here](https://github.com/assa77/DLS9-enumerator/releases).

## License

All contributions are made under the [GPLv3 license](http://www.gnu.org/licenses/gpl-3.0.en.html). See [LICENSE](LICENSE).

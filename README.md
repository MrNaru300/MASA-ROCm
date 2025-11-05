# MASA-ROCm extension

## What is it?

The MASA-ROCm extension is used with the MASA architecture to align biological
sequences in a AMD GPU.

## Compiling

The simplest way to compile the project is to use the following commands:

   ./configure
   make

More information can be found in the INSTALL file.

## Executing

The default command line parameters are inherited from the MASA-Core, but
new parameters were added by the MASA-ROCm extension. You can see all the
parameters passing the --help argument.

## Downloading

The source code of this project can be downloaded from the git repository:

   git clone <https://github.com/MrNaru300/MASA-ROCm>

Before compiling the project, execute the autoreconf script:

   autoreconf --force --install

## Development

We recommend to read the MASA doxygen documentation before creating any new
feature or extension.

## Copyright

**Copyright (c) 2010-2015 Edans Sandes**
**Copyright (c) 2025 Bruno Santiago de Oliveira (Modifications for ROCm)**

This file and all the files in the sub directories are part of **MASA-ROCm**,
which is based on the original MASA-CUDAlign project.

This file and all the files in the sub directories are part of MASA-ROCm
project.

MASA-ROCm is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

MASA-ROCm is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MASA-ROCm.  If not, see <http://www.gnu.org/licenses/>.

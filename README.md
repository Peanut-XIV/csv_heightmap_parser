# CSV Parser

## Introduction

This is a C program made to parse csv files of numeric values, sub-sample them
at 1/2 the scale in both direction and slice them into smaller tiles.

## Usage

Building the project or downloading the binary will give you an executable file
named `parser` or `parser.exe`. You can use the executable directly like so:

```sh
# in bash
path/to/parser path/to/config/file
```

```cmd
REM in windows cmd
C:\path\to\parser.exe path\to\config\file
```

In the example folder, you will find a template configuration file with
comments. You can copy and modify it as you will.

Alternatively, you can use the example python script located in the same
directory. It is an example use case of the executable that you can run,
provided you have python 3.10 or higher.

> Note: You will need to modify both the python script or config file to point
to the location of your executable, source file destination file and location
of the config file to create.

## Building from source

If you wish to build the project from source, the project has a very simple
structure. It has however a `CmakeLists.txt` file and you can use cmake if you
have it. You can build the project for your own computer, although I tried to
cross compile the project but could not make it work.

Here is how you can use cmake:

```sh
cd path/to/csv_heightmap_parser
cd ..
mkdir parser_build
cmake -S csv_heightmap_parser -B parser_build
cmake --build ./parser_build
```

## Issues?

If you have any problem with the project, please create an issue in the "Issue"
section of this github project.

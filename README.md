# mattime

mattime: a lightweight CLI utility to log your hours of progress

## Installation

Clone the repository, and install the binary with `cmake`.
Note that installing typically needs `sudo` privileges.

``` bash
git clone https://github.com/salad-dressing/mattime.git

cd mattime

# Specify the source and build directories.
cmake -S . -B build

# Build mattime and install the binary into the standard bin location.
sudo cmake --build build/ --target install
```

## Usage

Usage: `mattime [OPTION] ...`

Options:
```
  -h, help              Displays the help page
  -a, add               Record an entry with the specified number of hours
  -t, total             Displays the total number of hours and last updated
  -s, show              Show the last 10 entries
  -f, force             Force-set the total hours to the specified value
  -u, undo              Remove the last entry
  -r, reset             Reset the entire log
```
Examples:
``` 
  mattime add 10        Adds 10 hours to the total
  mattime force 50      Total hours is now set to 50
```

## Configuration

`mattime` aims to be highly customisable via `include/config.h`. After making 
any changes, `mattime` can be easily rebuilt and reinstalled by following the 
instructions from before.

### Developers

Developers can choose to create a debug build of `mattime`, enabling compiler 
warnings. This can be done by specifying the `CMAKE_BUILD_TYPE` flag as 
follows:

``` bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

The install step is then the same as in Release mode.

### Feedback

For feedback or issues, please report to the developer: saladdressing@mail.com
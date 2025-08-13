# mattime

mattime: a lightweight CLI utility to log hours of progress

## Installation

Clone the repository, and run the `install` script within it as superuser.
```
git clone https://github.com/salad-dressing/mattime.git
cd mattime
sudo ./install.sh
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
For feedback or issues, please report to the developer: saladdressing@mail.com

## Configuration

`mattime` aims to be highly customisable via `include/config.h`. After making 
any changes, `mattime` can be easily rebuilt by running the `install` script.


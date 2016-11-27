# mvi
Minimal vi clone

## Commands
Only the most basic functionality is welcomed:

	:o file - open file [*]
	:q      - quit
	:q!     - force quit
	:w file - write to file
	a       - append [*]
	dd      - delete line [*]
	h       - left
	i       - insert
	j       - down
	k       - up
	l       - right
	x       - delete character [*]

[*] Not implemented. Should it be?

Searching, substituting, etc does not have a place here.
Such could rather settle in a fork.

## Dependencies
* ncursesw
* [libutf](http://git.suckless.org/libutf/)

I have plans to remove the ncurses dependency.

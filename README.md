# mvi
Minimal vi clone

WARNING! Work in progress.

## Commands
Only the most basic functionality is welcomed:

	:q      - quit
	:q!     - force quit
	:w file - write to file
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

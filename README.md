# mvi
Minimal vi clone

WARNING! Work in progress.

## Commands
Only the most basic functionality is welcomed:

	:q        - quit
	:q!       - force quit
	:w [file] - write to file
	dd        - delete line [*]
	h,j,k,l   - left, down, up, right
	i         - insert
	x         - delete character [*]

[*] Not implemented. Should it be?

Search, substitute, etc, should rather be in a fork.

## Dependencies
* ncursesw (I plan to remove this dependency)
* [libutf](http://git.suckless.org/libutf/)

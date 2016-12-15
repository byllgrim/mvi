# mvi
Minimal vi clone (WIP)

## Commands

	:d        - delete line
	:q        - quit
	:q!       - force quit
	:w [file] - write to file
	h,j,k,l   - left, down, up, right
	i         - insert

## Dependencies
* ncursesw (I plan to remove this dependency)
* [libutf](http://git.suckless.org/libutf/)

## Description
This is supposed to be a very minimal vi clone.
Only a tiny subset of commands are supported.
Search, substitute, etc, should rather be in a fork.

Some behaviour is slightly different.
For instance moving to the end of a line,
or moving downwards within a line.

Beware that it is still a work in progress.

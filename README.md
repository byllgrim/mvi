# mvi
Minimal vi-like text editor

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
The editor is very minimal, so few commands are supported.
Search, substitute, etc, should rather be in a fork.

Its behaviour is not identical to vi!

Aside from a few bugs,
it is [fully functional](http://i.imgur.com/qJ2VReC.jpg).

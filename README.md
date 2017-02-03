# mvi
Minimal vi-like text editor in ~500 SLOC.

A more advanced version is [svi](https://github.com/byllgrim/svi).

## Commands

	:d        - delete line
	:q        - quit
	:q!       - force quit
	:w [file] - write to file
	h,j,k,l   - left, down, up, right
	i         - insert mode
	ESC       - normal mode

## Dependencies
* ncursesw or [ansicurses](https://github.com/byllgrim/ansicurses)
  with `-lansicurses` and `#include <ansicurses.h>`
* [libutf](http://git.suckless.org/libutf/)

## Description
The editor is very minimal, so few commands are supported.
Search, substitute, etc, should rather be in a fork.

Note that it differs from vi's behaviour!

mvi is [fully functional](http://i.imgur.com/qJ2VReC.jpg),
but please report bugs if you find any.

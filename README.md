# mvi

Minimal vi-like text editor in ~500 SLOC.

mvi is less advanced than [svi](https://github.com/byllgrim/svi).

## Commands

	:d        - delete line
	:q        - quit
	:q!       - force quit
	:w [file] - write to file
	h,j,k,l   - left, down, up, right
	i         - insert mode
	ESC       - normal mode

## Dependencies

* ncursesw
  (or [ansicurses](https://github.com/byllgrim/ansicurses) with `-lansicurses`
  and `#include <ansicurses.h>`)
* [libutf](http://git.suckless.org/libutf/)
  (another compatible libutf [here](https://github.com/cls/libutf))

## Description

The editor is very minimal, so few commands are supported.
Search, substitute, etc, should rather be in a fork.

mvi is [fully functional](http://i.imgur.com/qJ2VReC.jpg).

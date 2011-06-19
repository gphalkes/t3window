/** @mainpage

@section Introduction

The libt3window library provides functions for manipulating the terminal and
for creating (possibly overlapping) windows on a terminal. libt3window can be
used instead of (n)curses for drawing on the terminal. libt3window provides
the following features:

  - (Overlapping) windows for drawing. Overlapping windows hide windows deeper
	in the window stack.
  - Clipping of windows to the size of the parent window.
  - UTF-8 used internally, which is converted to the terminal encoding before
    output. libt3window depends on libt3unicode for UTF-8 processing and
	libtranscript for character set conversion.
  - Provides easy access to the most needed terminal functionality.
  - Small code size.

libt3window is part of the <A HREF="http://os.ghalkes.nl/t3/">Tilde Terminal
Toolkit (T3)</A>.

@section Example

The example below shows a small program which displays two overlapping windows.
When the user presses a key the front window is hidden, showing the previously
partially obscured window. To make the windows clearly visable a box is drawn
around each window.

@note
This program does not check whether the terminal supports the ::T3_ATTR_REVERSE
attribute. The terminal capabilities should be checked through ::t3_term_get_caps.

@include example.c

*/

NOTES ON GLK HUGO
-----------------

This is a port of Hugo to Glk, the multi-platform interface layer designed
by Andrew Plotkin, which enables Hugo to run on platforms to which it has 
not been directly ported.

Because Glk does not itself support all of Hugo's features, the limitations
of Glk Hugo on various platforms may include any or all of:

	- No graphics display
	- No sound or music
	- No colored text
	- No arbitrary window positioning
	- Main display limited to a proportional font
	- Windows limited to fixed-fonts
	
However, most Hugo games should work, since Glk Hugo makes every attempt to
gracefully work around these features even if they are used.


ADDITIONAL NOTES:

- With mouse-based versions of Glk, it may be possible to change the window
focus by clicking in a different window.  If you are unable to type a
command when Glk Hugo seems to be waiting for one, try clicking on the input
window to restore focus.


-----
Kent Tessman
January 23, 1999
# README FILE
# BY XIANKAI YANG
# APRIL 13TH 2016

Brief Introduction:

This mini web server can handle multiple processes (client) accessing, and can automatically collect zombie processes by init process. All the functionalities are working normally.

How to run the program:

In the directory where the source file 'file_browser.c' and the make file 'Makefile' are, type the following commands:

	$ make
	$ ./file_browser test 1527

Notice that the two argument you type into should be valid. The first one indicates the working directory, which in this case is "test". Any port number from 1 to 9999 will do.

To clean up executable file and log files in test, type the following commands:

	$ make clean

To terminate the program, type CTRL + C.

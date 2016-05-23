# README FILE
# BY XIANKAI YANG
# APRIL 6TH 2016

Brief Introduction:

The reverse-engineered client takes input from the user and output the result from the server. The program only take 1, 2 and 3 as valid inputs. Other invalid integers or characters will be ignored in the input stream. Other basic functionalities are the same with the original client.

Improvement:

1. If the input stream has invalid string such as 'ad3e4', '2412', '@&#^', the program will automatically filter those strings and keep reading other valid strings.
2. Rapid input will not cause connection failure.

How to run the program:

In the directory where the source file 'client.c' and the make file 'Makefile' are, type the following commands:

	$ make
	$ ./client

To terminate the program, type CTRL + C.

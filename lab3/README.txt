# README FILE
# BY XIANKAI YANG
# APRIL 20TH 2016

******************** HOW TO BUILD EXECUTABLE FILES ********************
To generate the executable files, type the following commands: (Under the lab3/ directory)

	$ make

Two files: /server/app_server /client/app_client will be generated.
#Notice# Use make to build executable files on server machine and client machine separately instead of simply copying executable files, since different OS might have different compiler, e.g. Linux vs MacOS.

******************** HOW TO CLEAN EXECUTABLE FILES ********************
To clean the executable files, type the following commands: (Under the lab3/ directory)

	$ make clean

Two files: /server/app_server /client/app_client will be removed.

******************** HOW TO RUN EXECUTABLE FILES ********************
To execute these two programs, type the following commands: (Under the lab3/ directory)

	# first run the server program
	# on the server machine, e.g. tahoe.cs.dartmouth.edu
	$ server/app_server
	waiting for connection
	
	# after server program starts to wait for connection, run the client program
	# on the other remote machine, and type in the server hostname, e.g. tahoe.cs.dartmouth.edu
	$ client/app_client
	Enter server name to connect:tahoe.cs.dartmouth.edu

Then the two program will interact with each other with print log like the following:

	#app_server
	[tahoe:lab3] 107) server/app_server 
	waiting for connection
	SRT Server: Initialization Completed.
	SRT Server: TCB[0] of Port (88) Created.
	SRT Server: TCB[0] State (CLOSED) ==> (LISTENING)
	SRT Server: TCB[0] Received SYN!
	SRT Server: TCB[0] State (LISTENING) ==> (CONNECTED)
	SRT Server: TCB[0] Sending SYNACK Success!
	seg lost!!!
	SRT Server: TCB[0] Accept Connection!
	SRT Server: TCB[1] of Port (90) Created.
	SRT Server: TCB[1] State (CLOSED) ==> (LISTENING)
	SRT Server: TCB[1] Received SYN!
	SRT Server: TCB[1] State (LISTENING) ==> (CONNECTED)
	SRT Server: TCB[1] Sending SYNACK Success!
	SRT Server: TCB[1] Accept Connection!
	SRT Server: TCB[0] Received FIN!
	SRT Server: TCB[0] State (CONNECTED) ==> (CLOSEWAIT)
	SRT Server: TCB[0] Sending FINACK Success!
	SRT Server: TCB[1] Received FIN!
	SRT Server: TCB[1] State (CONNECTED) ==> (CLOSEWAIT)
	SRT Server: TCB[1] Sending FINACK Success!
	SRT Server: TCB[0] CLOSEWAIT_TIMEOUT!
	SRT Server: TCB[0] State (CLOSEWAIT) ==> (CLOSED)
	SRT Server: TCB[1] CLOSEWAIT_TIMEOUT!
	SRT Server: TCB[1] State (CLOSEWAIT) ==> (CLOSED)
	SRT Server: TCB[0] Closing Completed.
	SRT Server: TCB[1] Closing Completed.

	#app_client
	yangxk15s-mbp:lab3 yangxk15$ client/app_client 
	Enter server name to connect:tahoe.cs.dartmouth.edu
	SRT Client: Initialization Completed.
	SRT Client: TCB[0] of Port (87) Created.
	SRT Client: TCB[0] First Try: Sending SYN Success!
	SRT Client: TCB[0] State (CLOSED) ==> (SYNSENT)
	SRT Client: TCB[0] Received SYNACK!
	SRT Client: TCB[0] State (SYNSENT) ==> (CONNECTED)
	client connect to server, client port:87, server port 88
	SRT Client: TCB[1] of Port (89) Created.
	SRT Client: TCB[1] First Try: Sending SYN Success!
	SRT Client: TCB[1] State (CLOSED) ==> (SYNSENT)
	SRT Client: TCB[1] Retry [1]: Sending SYN Success!
	SRT Client: TCB[1] Received SYNACK!
	SRT Client: TCB[1] State (SYNSENT) ==> (CONNECTED)
	client connect to server, client port:89, server port 90
	SRT Client: TCB[0] First Try: Sending FIN Success!
	SRT Client: TCB[0] State (CONNECTED) ==> (FINWAIT)
	SRT Client: TCB[0] Received FINACK!
	SRT Client: TCB[0] State (FINWAIT) ==> (CLOSED)
	SRT Client: TCB[0] Closing Completed.
	SRT Client: TCB[1] First Try: Sending FIN Success!
	SRT Client: TCB[1] State (CONNECTED) ==> (FINWAIT)
	SRT Client: TCB[1] Received FINACK!
	SRT Client: TCB[1] State (FINWAIT) ==> (CLOSED)
	SRT Client: TCB[1] Closing Completed.

******************** Source/Header Files Organization ********************
In client directory:
	app_client.c - client application source file
	srt_client.c - str client side  source file
 	srt_client.h - srt client header file	
In server directory:
	app_server.c - server application source file
	srt_server.c - srt server side source file
	srt_server.h - srt server header file
In common directory:
	seg.h - segment header file
	seg.c - segment source file
	constants.h - constants used by SRT
#Notice# To make the include header file hierarchy clear, I moved all the necessary headers in /common/constant.h as the basic header file. The hierarchy is listed as following:

constant.h
#included by
seg.h (seg.c)
#included by
srt_client.h (srt_client.c app_client.c)		srt_server.h (srt_server.c app_server.c)

******************** PTHREAD_MUTEX_LOCK********************
To make sure there is no messy interleaving when operating on TCB's state, every time we try to read or change the state, we acquire the "pthread_mutex_lock" first, and before we exit the critical section, we release the "pthread_mutex_lock".

all: DartSync
DartSync: DartSync.c tracker/tracker.o peer/peer.o
	gcc -Wall -pedantic -std=c99 -std=gnu99 -g -m32 -pthread DartSync.c tracker/tracker.o peer/peer.o tracker/trackerpeerTable.o peer/peerpeerTable.o common/pkt.o common/fileTable.o -o DartSync
tracker/tracker.o: tracker/tracker.c tracker/trackerpeerTable.o common/pkt.o common/fileTable.o
	gcc -Wall -pedantic -std=c99 -std=gnu99 -g -m32 -c -pthread tracker/tracker.c -o tracker/tracker.o
peer/peer.o: peer/peer.c peer/peerpeerTable.o common/pkt.o common/fileTable.o
	gcc -Wall -pedantic -std=c99 -std=gnu99 -g -m32 -c -pthread peer/peer.c -o peer/peer.o
tracker/trackerpeerTable.o: tracker/trackerpeerTable.c tracker/trackerpeerTable.h
	gcc -Wall -pedantic -std=c99 -std=gnu99 -g -m32 -c tracker/trackerpeerTable.c -o tracker/trackerpeerTable.o
peer/peerpeerTable.o: peer/peerpeerTable.c peer/peerpeerTable.h
	gcc -Wall -pedantic -std=c99 -std=gnu99 -g -m32 -c peer/peerpeerTable.c -o peer/peerpeerTable.o
common/pkt.o: common/pkt.c common/pkt.h common/fileTable.h
	gcc -Wall -pedantic -std=c99 -std=gnu99 -g -m32 -c common/pkt.c -o common/pkt.o
common/fileTable.o: common/fileTable.c common/fileTable.h common/constants.h
	gcc -Wall -pedantic -std=c99 -std=gnu99 -g -m32 -c common/fileTable.c -o common/fileTable.o

clean:
	rm -rf common/*.o
	rm -rf peer/*.o
	rm -rf tracker/*.o
	rm -rf DartSync 

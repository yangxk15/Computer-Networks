#include "peer/peer.h"
#include "tracker/tracker.h"

int main(int argc, char *argv[]) {
	if (argc == 2 && 0 == strcmp(argv[1], "-tracker")) {
		tracker_main();
	}
	else if(argc == 2 && 0 == strcmp(argv[1], "-peer")) {
		peer_main();
	}
	else {
		printf("Usage: DartSync [-tracker | -peer]\n");
		return 1;
	}
	return 0;
}



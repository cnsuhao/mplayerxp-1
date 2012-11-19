#include "libmpsub/subreader.h"

#ifdef DUMPSUBS
int main(int argc, char **argv) {  // for testing

    int i,j;
    subtitle *subs;
    subtitle *egysub;

    if(argc<2){
	printf("\nUsage: subreader filename.sub\n\n");
	exit(1);
    }
    sub_cp = argv[2];
    subs=sub_read_file(argv[1]);
    if(!subs){
	printf("Couldn't load file.\n");
	exit(1);
    }

    list_sub_file(subs);

    return 0;
}
#endif

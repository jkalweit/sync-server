#include <stdio.h>
#include <stdlib.h>

#include "webserver.h"



int main (int argc, char *argv[])
{
    sync_start_server("8080");
	return EXIT_SUCCESS;
}

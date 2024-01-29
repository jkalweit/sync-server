#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>

#include "webserver.h"
#include "utils.h"


struct Stepper {
	char name[50];
	int position;
	int target_position;
};

struct Stepper xAxis;
struct Stepper yAxis;

int
move_stepper(struct Stepper* stepper) {
	int diff = stepper->target_position - stepper->position;
	if(diff == 0) return 0;
	int step = 0;
	if(diff > 0) step = 1;
	if(diff < 0) step = -1;
	printf("Moving %s %d->%d %d\n", stepper->name, stepper->position, stepper->target_position, step);
	stepper->position += step;	
}

void intHandler(int dummy) {
	WEBSERVER_RUNNING = 0;
}

int 
handle_favicon(Request *request) {
	webserver_send404(request->socketfd);
}

int
handle_index(Request *request) {
	printf("Test: [%s]\n", request->request);
	webserver_send_file(request->socketfd, "index.html");
}


int
send_ping() {
	// char response[100];
	move_stepper(&xAxis);
	move_stepper(&yAxis);
	// sprintf(response, "X:%d:%d Y:%d:%d", xAxis.position, xAxis.target_position, yAxis.position, yAxis.target_position);
	// websockets_broadcast(response);
}

void *
PrintHello(void *threadid) {
	while(1) {
		// printf("Hello World!\n");
		send_ping();
		usleep(10);
	}
	printf("Done!\n");
	pthread_exit(NULL);
}


int
handle_default(WebsocketMessage *msg) {

	//xAxis.target_position += 1000;
	//printf("Handle test: [%d]\n", xAxis.target_position);

	/*
	   if(strcmp(msg->cmd, "get") == 0) {
	   printf("Handle Get: [%d] [%s] [%s]\n", msg->sfd, msg->channel, msg->data);
	   handle_get(msg);
	   } else if(strcmp(msg->cmd, "put") == 0) {
	   printf("Handle Put: [%d] [%s] [%s]\n", msg->sfd, msg->channel, msg->data);
	   fprintf(save_fp, "%s %s\n", msg->cmd, msg->data);
	   put(msg->data);
	   } else {
	   printf("Unknown cmd: [%s] [%s] [%s]\n", msg->cmd, msg->channel, msg->data);
	   }
	   */

		
	printf("Msg received: [%s] [%s] [%s]\n", msg->cmd, msg->channel, msg->data);
	//websockets_send_message(msg->sfd, msg->data);
	websockets_broadcast(msg->channel);
}


int 
main (int argc, char *argv[])
{
	/*
	strcpy(xAxis.name, "X-Axis");
	xAxis.position = 0;
	xAxis.target_position = 50;

	strcpy(yAxis.name, "Y-Axis");
	yAxis.position = 20;
	yAxis.target_position = 10;
	*/

	signal(SIGINT, intHandler);

	//pthread_t tid; 
	//pthread_create(&tid, NULL, PrintHello, NULL);

	webserver_init();

	webserver_add_route("/", handle_index); 
	webserver_add_route("/favicon.ico", handle_favicon); 

	websockets_add_channel("default", handle_default);

	webserver_start("8080");
	printf("Program finished.\n");
	return EXIT_SUCCESS;
}

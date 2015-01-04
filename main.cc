#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <zmq.h>
#include <string.h>
#include <assert.h>

#include <signal.h>
#include <pthread.h>

#define MSGCANCOMMIT 1
#define MSGYES 2
#define MSGNO 3
#define MSGPRECOMMIT 4
#define MSGACK 5
#define MSGDOCOMMIT 6
#define MSGABORT 9

#define STATEFREE 10
#define STATEWAITING 11
#define STATEPREPARED 12

int m=0,state=STATEFREE;
int *msg;
int rank, size, chanceOfWantCommit=25,chanceOfNo=25,changeOfError=15;
void *context_Server = zmq_ctx_new ();
void *responder_Server = zmq_socket (context_Server, ZMQ_PULL);
int rc_Server = zmq_bind (responder_Server, "tcp://192.168.2.1:5555");

void *context_Client = zmq_ctx_new ();
void *requester_Client = zmq_socket (context_Client, ZMQ_PUSH);

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_t thread,thread_timer;

void *timerfunction1(void *i){
 	int a = *((int *) i);
  free(i);
	usleep(100000);
	pthread_mutex_lock(&mutex1);
	if(state!=a){
		pthread_mutex_unlock(&mutex1);
		return NULL;
	}
  printf("TIMEOUT - ABORTED\n");
	state=STATEFREE;
	pthread_mutex_unlock(&mutex1);
}
void *timerfunction2(void *i){
	int a = *((int *) i);
	free(i);
	usleep(100000);
	pthread_mutex_lock(&mutex1);
	if(state!=a){
		pthread_mutex_unlock(&mutex1);
		return NULL;
	}
	state=STATEFREE;
	printf("TIMEOUT - DOCOMMIT\n");

	pthread_mutex_unlock(&mutex1);
}

void *sendWantCommit(void*){
	int tmp=MSGCANCOMMIT;
  while(true){
    int t=200000*((int)rand()%10);
    // printf("czas %d\n",t);
  	usleep(t);
    // printf("-------------------------timer\n");
    if(rand()%100<chanceOfWantCommit){
  	pthread_mutex_lock(&mutex1);
    printf("send wantCommit\n");
  	zmq_send (requester_Client, &tmp, sizeof(tmp), ZMQ_NOBLOCK);
  	pthread_mutex_unlock(&mutex1);
    }
  }
};
void wantCommit()
{
		pthread_create(&thread,NULL,sendWantCommit,NULL);
};
void abort(){
  // pthread_mutex_lock(&mutex1);
  state=STATEFREE;
  // pthread_mutex_unlock(&mutex1);
};
void recivCANCOMMIT(int msg)
{
	int tmp=0;
	pthread_mutex_lock(&mutex1);
	if(state==STATEFREE)
	{
		state=STATEWAITING;
		tmp=MSGYES;
		int *arg =(int*) malloc(sizeof(*arg));
    *arg = state;
    pthread_create(&thread_timer, NULL, timerfunction1, arg);
	}else{
		tmp=MSGNO;
	}
  if(rand()%100<20)
    {
      tmp=MSGNO;
      abort();
    }
	//pthread_mutex_unlock(&mutex1);
	printf("send %d\n",tmp);
	//pthread_mutex_lock(&mutex1);
	zmq_send (requester_Client, &tmp, sizeof(tmp), ZMQ_NOBLOCK);
//	state=STATEWAITING;
	pthread_mutex_unlock(&mutex1);

}
void recivPRECOMMIT(int msg)
{
	pthread_mutex_lock(&mutex1);
	if(state!=STATEWAITING)
	{
		pthread_mutex_unlock(&mutex1);
		return;
	}
	pthread_cancel(thread_timer);

	int tmp=MSGACK;
	state=msg;
	printf("send %d\n",tmp);
	zmq_send (requester_Client, &tmp, sizeof(tmp), ZMQ_NOBLOCK);
	state=STATEPREPARED;
	int *arg =(int*)malloc(sizeof(*arg));
	*arg = state;
	pthread_create(&thread_timer, NULL, timerfunction2, arg);
	pthread_mutex_unlock(&mutex1);
}
void recivDOCOMMIT()
{
	pthread_mutex_lock(&mutex1);
	pthread_cancel(thread_timer);
	if(state!=STATEPREPARED)
	{
		pthread_mutex_unlock(&mutex1);
		return;
	}
	state=STATEFREE;
	pthread_mutex_unlock(&mutex1);
	printf("DOCOMMIT\n");
}

void controlUnit(int msg)
{
	switch(msg){
		case MSGCANCOMMIT: {
      if(rand()%100<changeOfError){
        printf("RECIV CANCOMMIT - ERROR\n");
        pthread_mutex_lock(&mutex1);
        state=STATEFREE;
        pthread_mutex_unlock(&mutex1);
      }else{
        printf("RECIV CANCOMMIT\n");
        recivCANCOMMIT(msg);
      }
      break;
      }
		case MSGPRECOMMIT: {
      if(rand()%100<changeOfError){
        printf("RECIV PRECOMMIT - ERROR\n");
        pthread_mutex_lock(&mutex1);
        state=STATEFREE;
        pthread_mutex_unlock(&mutex1);
      }else{
        printf("RECIV PRECOMMIT\n");
        recivPRECOMMIT(msg);
      }
      break;
      }
		case MSGDOCOMMIT: {
      printf("RECIV DOCOMMIT\n");
      recivDOCOMMIT();
      break;
      }
		case MSGABORT: {
      printf("RECIV ABORT\n");
      pthread_mutex_lock(&mutex1);
      state=STATEFREE;
      pthread_mutex_unlock(&mutex1);
      break;
      }
	}
};

int main (int argc, char* argv[])
{
	int tmpwiad[1];
	zmq_connect (requester_Client, "tcp://192.168.2.2:5555");
	int seed=time(NULL)+5*11311*time(NULL);
	srand(seed);
	printf( "Hello world from process \n" );

	m=0;

		wantCommit();
	while(true){
		msg=(int*)malloc(sizeof(int));
		// printf("asd\n");
		zmq_recv (responder_Server, msg, sizeof(int), 0);
		// printf("asd\n");
		int tmp=*msg;
		printf("dostalem wiadomosc %d\n",tmp);
		controlUnit(tmp);
		free(msg);
	}
	usleep(110000);
	zmq_close (requester_Client);
	zmq_ctx_destroy (context_Client);
	return 0;

	while(true){};
}

//Author: Tyler Yamashiro, Jared Mead, Jeyte Hagaley
#include <iostream>
#include <string>
#include <cstring>
#include <stdlib.h>
#include <sys/types.h> // size_t, ssize_t
#include <sys/socket.h> // socket funcs
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h> // htons, inet_pton
#include <unistd.h> // close
#include <netdb.h>
#include "pthread.h"
#include <stdio.h>
#include <sstream>
using namespace std;
pthread_mutex_t mutex;
const int MAXPENDING = 50;//Max pending new connections
const string DEFAULTPORT = "80";
const string DEFAULTVERSION = "HTTP/1.0";
const string CONNECTCLOSE = "Connection: close";
const int MAX_CONNECTIONS = 20;
int CURRENT_CONNECTIONS = 0;
//Client socket for passing in the multi threading
struct ThreadArgs{
  int clientSock;
};

void fiveErr(int socket){
  char error500[30] = "500 'Internal Error' \n";
  int bytesSentcurrent = 0;
 while(bytesSentcurrent < sizeof(error500)){
    int bytesSent = send(socket,(void*) &error500,
			 sizeof(error500), MSG_NOSIGNAL);
    bytesSentcurrent +=bytesSent;
  }
  pthread_mutex_lock(&mutex);
  CURRENT_CONNECTIONS--;
  pthread_mutex_unlock(&mutex);
  pthread_detach(pthread_self());
  close(socket);
  pthread_exit(0);
}
void* threadMain(void* args)
{
  int urlIgnroe = 2;
  int length = 2040 ;
  int HtmlResponsLength = 500000;
  int errorMessageLength = 20;
  struct ThreadArgs *threadArgs = (struct ThreadArgs *) args;
  int clientSock = threadArgs -> clientSock;
  delete threadArgs;
  char buffer[length];
  char error500[30] = "500 Internal Error \n";
  memset(&buffer,'\0',length);
  char* buffer_pointer = buffer;
  //Recieve the GET request from telnet and stop recieving when the
  // \n character is found
  while(length){
    int bytesRecv = recv(clientSock,buffer_pointer,length,0);
    if(bytesRecv <= 0){
      fiveErr(clientSock);
    }
    length -= bytesRecv;
    buffer_pointer += bytesRecv;
    if(buffer[strlen(buffer)-1] == '\n'
        &&buffer[strlen(buffer)-2] == '\r'
        &&buffer[strlen(buffer)-3] == '\n'
        &&buffer[strlen(buffer)-4] == '\r'){
      break;
    }
  }

  //--------------------------------------------------------------------------
  //Parse the buffer using string stream and getlines.
  string requestBuffer =  buffer;
  stringstream ss(requestBuffer);
  string get;
  string url;
  string scheme;
  string version;
  string path;
  string portNum;
  getline(ss,get,' ');
  getline(ss,scheme,':');
  ss.ignore(2);
  getline(ss,url, '/');
  getline(ss,path,' ');
  getline(ss,version,' ');
  path.insert(0,"/");
  //replace with HTTP/1.0 automatically
  if(scheme != "http"){
    fiveErr(clientSock);
  }
  size_t findVer = version.find("HTTP/1.0");
  if(findVer != std::string::npos){
    version = DEFAULTVERSION;
  }
  else{
    fiveErr(clientSock);
  }

  //See if there is aport number included in teh url
  size_t found = url.find(':');
  //if there is a : present that means there is a port number
  //assign that port number to portNum if not send the default port for
  //html:80
  if(found != std::string::npos){
    stringstream ssi(url);
    ssi.ignore(256,':');
    getline(ssi,portNum,'/');
    ssi.clear();
    getline(ss,url,':');
  }
  else{
    portNum = DEFAULTPORT;
  }
  if(get != "GET"){
    fiveErr(clientSock);
  }
  //---------------------------------------------------------------------------
  //use get addrInfo to find the IP and connect to the server
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  //Create linked list of addinfo structs to hold sockaddr info of avail
  //ports on the given IP.
  if((rv = getaddrinfo(url.c_str(),portNum.c_str(), &hints, &servinfo)) != 0){
    cout << "no sever found" << endl;
    // freeaddrinfo(servinfo);
    fiveErr(clientSock);
  }
  //loop through all the structs and connect to the first one it finds
  for(p = servinfo; p != NULL; p = p->ai_next){
    if(p == NULL){
      // freeaddrinfo(servinfo);
      fiveErr(clientSock);
    }
    if((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1){
      perror("socket");
      continue;
    }
    if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
      perror("connect");
      continue;
    }
    //connected to a server
    break;
  }
  freeaddrinfo(servinfo);
  //if failed to connect to a server error
  if(sockfd <= 0){
    fiveErr(clientSock);
  }
  //-------------------------------------------------------------------------
  //format the GET request
  stringstream getRequest;
  getRequest << get << " " << path << " " << version << "\r\n"
             << "Host: " << url << "\r\n" << CONNECTCLOSE<<"\r\n\r\n";

  //send the get request to the server
  string HtmlR = getRequest.str();
  int strLeng = HtmlR.length();
  char HtmlRequest[strLeng];
  memset(&HtmlRequest,'\0',strLeng);
  strcpy(HtmlRequest,HtmlR.c_str());
  int bytesSentcurrent = 0;
  while(bytesSentcurrent < sizeof(HtmlRequest)){
    int bytesSent = send(sockfd,(void*) &HtmlRequest,
                  sizeof(HtmlRequest),MSG_NOSIGNAL);
    bytesSentcurrent +=bytesSent;
    if(bytesSent < 0){
      fiveErr(clientSock);
    }
  }
  //get the request from the html
  char HtmlResponse[HtmlResponsLength];
  memset(&HtmlResponse,'\0',HtmlResponsLength);
  char *hp = HtmlResponse;
  while(HtmlResponsLength){
    int bytesRecv = recv(sockfd,hp,HtmlResponsLength,0);
    if(bytesRecv <= 0 ){
      break;
    }
    HtmlResponsLength -= bytesRecv;
    hp += bytesRecv;
  }
  bytesSentcurrent = 0;
  while(bytesSentcurrent < sizeof(HtmlResponse)){
    int bytesSent = send(clientSock,(void*)&HtmlResponse,
                                      sizeof(HtmlResponse),MSG_NOSIGNAL);
    if(bytesSent < 0){
      fiveErr(clientSock);
    }
    bytesSentcurrent+=bytesSent;
  }

  pthread_mutex_lock(&mutex);
  CURRENT_CONNECTIONS--;
  pthread_mutex_unlock(&mutex);
  pthread_detach(pthread_self());
  close(clientSock);
}

int main(int argc, char* argv[])
{
  pthread_mutex_init(&mutex,NULL);
  if(argc != 2 ){
    cout << "Invalid arguments now exiting..." << endl;
    return 0;
  }
  unsigned short servPort = atoi(argv[1]);
  int server_sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  if(server_sock<0){
    cerr << "Error with socket...now exiting" << endl;
    exit(-1);
  }
  struct sockaddr_in servAddr;
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(servPort);
  int status = bind(server_sock,(struct sockaddr*) &servAddr,sizeof(servAddr));
  if(status < 0){
    cerr << "Error with bind...now exiting" << endl;
    exit(-1);
  }
  status = listen(server_sock,MAXPENDING);
  if(status<0){
    cerr << "Error with listen now exiting" << endl;
    exit(-1);
  }
  while(true){
    if(CURRENT_CONNECTIONS < MAX_CONNECTIONS){
      //Accept connection from client
      struct sockaddr_in clientAddr;
      socklen_t aLen = sizeof(clientAddr);
      int client_sock;
      client_sock = accept(server_sock, (struct sockaddr*) &clientAddr,&aLen);
      if(client_sock < 0) exit(-1);
      //create and initialize argument structs
      struct ThreadArgs *threadArgs;
      threadArgs = new struct ThreadArgs;
      threadArgs -> clientSock = client_sock;
      //create client thread
      pthread_mutex_lock(&mutex);
      CURRENT_CONNECTIONS++;
      pthread_mutex_unlock(&mutex);
      pthread_t threadID;
      status = pthread_create(&threadID, NULL, threadMain,
                            (void*) threadArgs);
      if(status != 0) exit(-1);

    }
}
  return 0;
}

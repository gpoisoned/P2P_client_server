#define _GNU_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h> 
#include <sys/select.h>
#include <netdb.h>

#define true 1
#define false 0
#define MAXMESGSIZE 4096
#define MAXCONNECTIONS 10 
#define MAXNUMFILES 1000
#define MAXBUFFSIZE 30000

struct record{
        char name[30];
        char fileSize[5];
        char fileOwner[3];
        char ownerIp[15];
        char ownerPort[8];
};

struct session{
        int connectfd;
        char cliName[3];
};

int check_session_exists(struct session conRec[],int max,struct session tmp){
        for(int i=0;i<max;i++){
                if(conRec[i].connectfd >0){
                        if(strncmp(conRec[i].cliName, tmp.cliName,strlen(tmp.cliName))==0){
                                return true; 
                        } 
                } 
        }
        return false;
}

void delete_from_database(struct record db[],int index, char* name){
        for(int i=0;i<index;i++){
                if(strncmp(db[i].fileOwner,name,strlen(name))==0){
                        // Delete entry:
                        memset(&db[i].name,'0',sizeof(db[i].name));
                        memset(&db[i].fileSize,0,sizeof(db[i].fileSize));
                        memset(&db[i].fileOwner,0,sizeof(db[i].fileOwner));
                        memset(&db[i].ownerIp,0,sizeof(db[i].ownerIp));
                        memset(&db[i].ownerPort,0,sizeof(db[i].ownerPort));
                }
        }
}

int store_to_database(struct record db[],int index,char* msg){
        char cliName[3];
        char cliIp[15];
        char cliPort[8];
        
        int i=0,j=0;
        int tokLen=0;
        char* tokens;

        printf("Storing into the database\n");
        //Parse the message:
        tokens = strtok(msg,","); 
        while(tokens !=NULL){
                tokLen = strlen(tokens);
                if(i==0){
                        strncpy(cliName,tokens, tokLen);
                        cliName[tokLen]='\0'; 
                        i++;
                } 
                else if(i ==1){
                        strncpy(cliIp,tokens,tokLen);
                        cliIp[tokLen]='\0';     
                        i++;
                }
                else if(i==2){
                        strncpy(cliPort,tokens,tokLen);
                        cliPort[tokLen]='\0';
                        i=-1; // end 
                }
                else {
                        if(j==0){
                                strncpy(db[index].fileOwner,cliName,strlen(cliName));
                                strncpy(db[index].ownerIp,cliIp,strlen(cliIp));
                                strncpy(db[index].ownerPort,cliPort,strlen(cliPort));

                                strncpy(db[index].name,tokens,tokLen);
                                db[index].name[tokLen]='\0';
                                j++;
                        }
                        else if(j ==1){
                                strncpy(db[index].fileSize,tokens,tokLen);  
                                db[index].fileSize[tokLen]='\0';
                                j=0;
                                index++;
                        }
                }
                tokLen = 0;
                tokens = strtok(NULL,",");
        }

        printf("value of index = %d\n",index);
        for(int i=0;i<index;i++){
                if(strncmp(db[i].name,"0",1) !=0){
                        printf("%s %s %s %s %s\n",db[i].name,db[i].fileSize,db[i].fileOwner,db[i].ownerIp,db[i].ownerPort); 
                }
        }
        return index;

}

void generate_master_file_list(char* buffer,struct record db[],int index){
        printf("Generating masterfile list\n");
        for(int i=0;i<index;i++){
                if(strncmp(db[i].name,"0",1) !=0){
                        strncat(buffer,db[i].name,strlen(db[i].name));
                        strncat(buffer,",",1);
                        strncat(buffer,db[i].fileSize,strlen(db[i].fileSize));
                        strncat(buffer,",",1);
                        strncat(buffer,db[i].fileOwner,strlen(db[i].fileOwner));
                        strncat(buffer,",",1);
                        strncat(buffer,db[i].ownerIp,strlen(db[i].ownerIp));
                        strncat(buffer,",",1);
                        strncat(buffer,db[i].ownerPort,strlen(db[i].ownerPort));
                        strncat(buffer,",",1);
                }
        } 
        buffer[strlen(buffer)-1]='\0';
}

int setup_server(struct sockaddr_in serv, int port_num){
        int temp_fd = socket(AF_INET, SOCK_STREAM,0);  
        if(temp_fd <0){
                printf("Error in socket()\n");
                exit(1);
        }
        serv.sin_family = AF_INET;
        serv.sin_addr.s_addr=htonl(INADDR_ANY);
        // Setup the port number to be any available port
        serv.sin_port = htons(port_num);

        if(bind(temp_fd,(struct sockaddr*)&serv,sizeof(serv)) < 0){
                perror("Error with bind: bind()\n");
                exit(1);
        }

        if(listen(temp_fd,MAXCONNECTIONS)<0){
                perror("Error with listen: listen()\n");
                exit(1);
        }
       
        return temp_fd;
}


int check_if_exists(int fd, int connectfd[], int maxConnect){
        for(int i=0;i<maxConnect;i++){
                if(connectfd[i] > 0){
                        if(fd == connectfd[i]){
                                return true; 
                        }
                }
        }
        return false;
}

void print_num_active_client(int connectfd[],int maxConnect){
        int count =0;
        for(int i=0;i<maxConnect;i++){
                if(connectfd[i]>0){
                        count++; 
                } 
        }
        printf("Total active clients = %d\n",count);

}

int main(int argc, char* argv[]){
        if(argc < 2){
                printf("Usage: %s <server_port>",argv[0]);
                exit(1);
        } 
       
        // Variables declaration:
        int listenfd;
        int connectfd[MAXCONNECTIONS];
        struct sockaddr_in servAddr , tempSock;
        struct sockaddr_in cliAddr[MAXCONNECTIONS];

        socklen_t cliLen[MAXCONNECTIONS];
        socklen_t tempSockLen;
        int fileListChanged = false;
        int newListAvailable = false;

        // Table to store the MasterFile list
        struct record database[MAXNUMFILES];
        int dbIndex = 0;

        fd_set read_fds;
        int numActiveConnections=0;
        
        // Connection record to stor info about 
        // connect file descriptor and cliName
        struct session conRec[MAXCONNECTIONS];
        
        memset(&servAddr,'\0',sizeof(servAddr));
        memset(&tempSock,'\0',sizeof(tempSock));
        memset(&tempSockLen,'\0',sizeof(tempSockLen));
        for(int i=0;i<MAXCONNECTIONS;i++){
                memset(&cliAddr[i],'\0',sizeof(cliAddr[i]));  
                memset(&cliLen[i],'\0',sizeof(cliLen[i]));
                memset(&connectfd[i],'\0',sizeof(connectfd[i]));
                memset(&conRec[i].connectfd,0,sizeof(conRec));
                memset(&conRec[i].cliName,0,sizeof(conRec));
        }

        for(int i=0;i<MAXCONNECTIONS;i++){
                cliLen[i] = sizeof(cliAddr[i]);
                connectfd[i] = -1;
                conRec[i].connectfd = -1;
        }
        tempSockLen = sizeof(tempSock);

        listenfd = setup_server(servAddr, atoi(argv[1]));
        printf("Listenfd = %d\n",listenfd);
        
        // buffer to send the master file list:
        char sendMsg[MAXMESGSIZE];
        int recvBytes = 0;
        char recvBuff[MAXMESGSIZE];
        
        int opt = 1;
        if( setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
        {
                perror("setsockopt");
                exit(EXIT_FAILURE);
        }


        while(true){
                FD_ZERO(&read_fds);
                FD_SET(listenfd,&read_fds);                        

                for(int i=0;i<MAXCONNECTIONS;i++){
                        if(connectfd[i] > 0){
                                FD_SET(connectfd[i],&read_fds);
                        }
                }

                printf("---------------------------------------\n");
                printf("Server watiting for incoming connections\n");
                int retVal = select(FD_SETSIZE,&read_fds,NULL,NULL,NULL);
                if(retVal == -1){
                        perror("select()\n");
                        exit(1);
                }
                else if(retVal){
                        if(FD_ISSET(listenfd,&read_fds)){
                                int temp_fd = accept4(listenfd,(struct sockaddr*)&tempSock,&tempSockLen,SOCK_NONBLOCK);
                                if(temp_fd >0){
                                        printf("Got connection from client: ip %s, port %d\n",inet_ntoa(tempSock.sin_addr),
                                                        ntohs(tempSock.sin_port));
                                        //Get the message from the client:
                                        memset(recvBuff,'0',sizeof(recvBuff));
                                        if((recvBytes = recv(temp_fd,recvBuff, MAXMESGSIZE-1,0)) < 0){
                                                printf("Recv error\n");
                                                continue;
                                        }
                                        recvBuff[recvBytes] ='\0';

                                        printf("Received %d bytes\n",recvBytes);
                                        printf("%s\n",recvBuff);
                                        if(check_if_exists(temp_fd, connectfd, MAXCONNECTIONS)==false){
                                                //Client connecting to the server for the first time:               
                                                //Register the client to activeConnectionsList
                                                for(int i=0;i<MAXCONNECTIONS;i++){
                                                        if(connectfd[i] <= 0){
                                                                printf("Storing the connected client info\n");
                                                                connectfd[i] = temp_fd;
                                                                memcpy(&cliAddr[i],&tempSock,sizeof(tempSock));
                                                                memcpy(&cliLen[i], &tempSockLen,sizeof(tempSockLen));
                                                                break;
                                                        } 
                                                }

                                                struct session tmp;
                                                tmp.connectfd = temp_fd;
                                                strncpy(tmp.cliName,recvBuff,1);
                                                tmp.cliName[strlen(tmp.cliName)]='\0';

                                                // Check if this client Name already exists:
                                                // Exit the client if so:
                                                int exists = check_session_exists(conRec, MAXCONNECTIONS,tmp);
                                                if(exists){
                                                        //Exit connection      
                                                        printf("%s already exists\n",tmp.cliName);
                                                        char sendErr[]="error\0";
                                                        printf("%s\n",sendErr);
                                                        if(sendto(temp_fd,sendErr,strlen(sendErr),0,
                                                                                (struct sockaddr*)&tempSock,sizeof(tempSock)) < 0){
                                                                perror("Send error\n");
                                                                exit(1);
                                                        }
                                                        else{
                                                                printf("%d bytes sent to client\n",strlen(sendMsg)); 
                                                                // Client will respond with quit message
                                                                // which gets handled later
                                                        }
                                                        fileListChanged = false;
                                                        newListAvailable = false;

                                                }
                                                else{
                                                        // Add to the connection Record:
                                                        for(int i=0;i<MAXCONNECTIONS;i++){
                                                                if(conRec[i].connectfd <= 0){
                                                                        conRec[i].connectfd = temp_fd;
                                                                        strncpy(conRec[i].cliName,tmp.cliName,
                                                                                strlen(tmp.cliName));
                                                                        break;
                                                                } 
                                                        }
                                                        // Test add above:
                                                        for(int i=0;i<MAXCONNECTIONS;i++){
                                                                if(conRec[i].connectfd > 0){
                                                                        printf("%d %s\n",conRec[i].connectfd,
                                                                                        conRec[i].cliName);
                                                                } 
                                                        }

                                                        newListAvailable =true;
                                                        fileListChanged = true;
                                                }

                                        }

                                }

                        } 
                        else{
                                // Connection from the connected clients:
                                for(int i=0;i<MAXCONNECTIONS;i++){
                                        if(connectfd[i] >0){
                                                if(FD_ISSET(connectfd[i],&read_fds)){
                                                        //Get the message from the client:
                                                        memset(recvBuff,'0',sizeof(recvBuff));
                                                        if((recvBytes = recv(connectfd[i],recvBuff, MAXMESGSIZE-1,0)) < 0){
                                                                printf("Recv error\n");
                                                                continue;
                                                        }
                                                        recvBuff[recvBytes] ='\0';

                                                        printf("Received %d bytes\n",recvBytes);
                                                        printf("%s\n",recvBuff);
                                                                        
                                                        if(strncmp(recvBuff,"exit",4) == 0){
                                                                printf("Received command exit\n");
                                                                
                                                                printf("disconnected fd =%d\n",connectfd[i]);
                                                                // Remove all entries with that source from db
                                                                int hit;
                                                                for(int j=0;j<MAXCONNECTIONS;j++){
                                                                        int temp = conRec[j].connectfd;
                                                                        if(temp == connectfd[i]){
                                                                                hit = j;
                                                                                break;
                                                                        } 
                                                                }
                                                                printf("%s disconnected\n",conRec[hit].cliName);
                                                               // Delete entries from database:
                                                               delete_from_database(database, dbIndex,
                                                                               conRec[hit].cliName);

                                                               // Delete entries from session:
                                                                conRec[hit].connectfd = -1;
                                                                memset(&conRec[hit].cliName,'0',
                                                                                sizeof(conRec[hit].cliName));

                                                                // close the filehandle
                                                                close(connectfd[i]);
                                                                connectfd[i] = -1;

                                                                // Set the flag to changed
                                                                fileListChanged = true;

                                                                // Print the number of currently active clients:
                                                                print_num_active_client(connectfd, MAXCONNECTIONS);
                                                        }
                                                        else if(strncmp(recvBuff,"quit",4)==0){
                                                                printf("Received command quit\n"); 
                                                                close(connectfd[i]);
                                                                connectfd[i] = -1;
                                                                fileListChanged = false;

                                                                // Print the number of currently active clients:
                                                                print_num_active_client(connectfd, MAXCONNECTIONS);
                                                        }
                                                }
                                        }
                                
                                }
                        
                        }                        

                        if(fileListChanged){
                                if(newListAvailable){
                                        // Add to the databse: 
                                        dbIndex = store_to_database(database, dbIndex, recvBuff);
                                        newListAvailable = false;
                                }

                                // Send the master file list to all active peers:
                                //
                                memset(sendMsg,'\0',sizeof(sendMsg));
                                generate_master_file_list(sendMsg,database,dbIndex);
                                printf("%s\n",sendMsg);

                                for(int i=0;i<MAXCONNECTIONS;i++){
                                        if(connectfd[i] > 0){
                                                if(sendto(connectfd[i],sendMsg,strlen(sendMsg),0,
                                                                        (struct sockaddr*)&cliAddr[i],sizeof(cliAddr[i])) < 0){
                                                        perror("Send error\n");
                                                        exit(1);
                                                }
                                                else{
                                                        printf("%d bytes sent to client\n",strlen(sendMsg)); 
                                                }
                                        }

                                }
                                fileListChanged = false;
                                continue;
                        }

                        memset(recvBuff,'0',sizeof(recvBuff));
                        memset(&tempSock,'\0',sizeof(tempSock));
                        memset(&tempSockLen,'\0',sizeof(tempSockLen));
                        
                        continue;
                }

        }

        return 0;
}

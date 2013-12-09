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
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>
#include <fcntl.h>

#define true 1
#define false 0
#define MAXBUFFSIZE 30000
#define MAXCONNECTIONS 10 
#define MAXCMDSIZE 32
#define MAXMESGSIZE 4096
#define MAXNUMFILES 1000

struct record{
        char name[30];
        char fileSize[5];
        char fileOwner[3];
        char ownerIp[15];
        char ownerPort[8];
};

void show_database(struct record db[],int index){
        printf("FileName|| FileSize KB|| FileOwner|| Owner IP|| Owner Port\n");
        for(int i=0;i<index;i++){
                printf("%s|| %s|| %s|| %s|| %s\n",db[i].name,db[i].fileSize,db[i].fileOwner,db[i].ownerIp,db[i].ownerPort); 
        }
}

int clear_database(struct record db[],int index){
        for(int i=0;i<index;i++){
                memset(db[i].name,'\0',sizeof(db[i].name)); 
                memset(db[i].fileSize,'\0',sizeof(db[i].fileSize)); 
                memset(db[i].fileOwner,'\0',sizeof(db[i].fileOwner)); 
                memset(db[i].ownerIp,'\0',sizeof(db[i].ownerIp)); 
                memset(db[i].ownerPort,'\0',sizeof(db[i].ownerPort)); 
        }
        return 0;
}

// Function:
//      int get_file_owner_in_database(struct record db[], int dbIndex,char* file){
//
// Returns: the index to the databse where match found
int get_file_owner_in_database(struct record db[], int dbIndex,char* file){
        for(int i=0;i<dbIndex;i++){
                if(strncmp(db[i].name, file,strlen(file)) == 0){
                        return i;                
                }
        }
        return -1;
}

int add_master_fileList_to_database(struct record db[],int index, char* message){
        char* tokens;
        int i=0;
        int tokLen=0;
        printf("Got new master file list from the server\n");
        //Parse the message:
        tokens = strtok(message,","); 
        while(tokens !=NULL){
                tokLen = strlen(tokens);
                if(i==0){
                        strncpy(db[index].name,tokens, tokLen);
                        db[index].name[tokLen]='\0'; 
                        i++;
                } 
                else if(i ==1){
                        strncpy(db[index].fileSize,tokens,tokLen);
                        db[index].fileSize[tokLen]='\0';     
                        i++;
                }
                else if(i==2){
                        strncpy(db[index].fileOwner,tokens,tokLen);
                        db[index].fileOwner[tokLen]='\0';
                        i++; 
                }
                else if(i==3){
                        strncpy(db[index].ownerIp,tokens,tokLen);
                        db[index].ownerIp[tokLen]='\0';
                        i++; 
                }
                else if(i==4){
                        strncpy(db[index].ownerPort,tokens,tokLen);
                        db[index].ownerPort[tokLen]='\0';
                        i=0; 
                        index++;
                }
                tokLen = 0;
                tokens = strtok(NULL,",");
        }
        printf("FileName|| FileSize KB|| FileOwner|| Owner IP|| Owner Port\n");
        for(int i=0;i<index;i++){
                printf("%s|| %s|| %s|| %s|| %s\n",db[i].name,db[i].fileSize,db[i].fileOwner,db[i].ownerIp,db[i].ownerPort); 
        }
        return index;
}

int get_file_size(char* fname){
        struct stat f_stat;
        if(stat(fname, &f_stat) == -1){
                perror("stat()"); 
                exit(1);
        }
        return (size_t)f_stat.st_size;
}

// Function: get_files_from_current_dir(char* buffer)
//
//      This fills the "buffer" with the list of files in the current directory excluding 
//      "." and ".." in Linux
//
//      Return Value:
//              Returns the number of bytes filled in the buffer.
int get_files_from_current_dir(char* buffer){
        DIR *d;
        struct dirent *dir;
        d = opendir(".");
        if (d!=NULL){
                while((dir = readdir(d)) != NULL){
                        if((strncmp(dir->d_name,".",1)!=0)&& (strncmp(dir->d_name,"..",2)!=0)){
                                strncat(buffer,dir->d_name,strlen(dir->d_name));
                                strncat(buffer,",",1);
                                int temp = get_file_size(dir->d_name);
                                //printf("File size of %s = %d\n",dir->d_name,(temp/1024));
                                char buf[5];
                                sprintf(buf,"%d",temp/1024);
                                strncat(buffer,buf,strlen(buf));
                                strncat(buffer,",",1);
                        } 
                } 
        }
        // Fix for last comma
        buffer[strlen(buffer)-1]='\0';
        return strlen(buffer);
}

int generate_message(char *message, int listenfd, struct sockaddr_in local,char* cliName){
        socklen_t length = sizeof(local);
        getsockname(listenfd, (struct sockaddr*)&local,&length);
        int portNum = ntohs(local.sin_port);
        printf("Client listening to port number: %d\n",portNum);
        char buf[8];
        sprintf(buf,"%d",portNum);
        int portLen = strlen(buf);

        // Append the client Name to message
        int argLen = strlen(cliName);
        memcpy(message,cliName, argLen);  
        strncat(message,",",argLen);
        // Append client ip and port num
        strncat(message,inet_ntoa(local.sin_addr),strlen(inet_ntoa(local.sin_addr)));
        strncat(message,",",1);
        strncat(message,buf,portLen); 
        strncat(message,",",1);

        // Append files List:
        char filesList[1024];
        int lenFilesList = get_files_from_current_dir(filesList);
        strncat(message,filesList,lenFilesList);
       
        message[strlen(message)] ='\0';
        // Final message to be sent to the server
        printf("%s\n",message);
        printf("Length of message is: %d\n",strlen(message));
        return strlen(message);
}

int setup_connect(struct sockaddr_in servAddr,char* ipAddr,int portNum){
        int temp_fd = socket(AF_INET, SOCK_STREAM,0);  
        struct sockaddr_in serv;
        if(temp_fd <0){
                printf("Error in socket()\n");
                exit(1);
        }
        serv.sin_family = AF_INET;
        if(inet_pton(AF_INET, ipAddr, &serv.sin_addr) <= 0){
                printf("inet_pton error!\n");
                exit(1);
        }
        serv.sin_port = htons((unsigned short)portNum);

        if(connect(temp_fd,(struct sockaddr*)&serv,sizeof(serv)) < 0){
                        printf("Connect failed\n");
                        exit(1);
        }
        printf("sent to %s %d\n",inet_ntoa(serv.sin_addr),ntohs(serv.sin_port)); 
        memcpy(&servAddr,&serv,sizeof(serv));
        return temp_fd;
}

int setup_listen(struct sockaddr_in local){
        int temp_fd = socket(AF_INET, SOCK_STREAM,0);  
        if(temp_fd <0){
                printf("Error in socket()\n");
                exit(1);
        }
        local.sin_family = AF_INET;
        local.sin_addr.s_addr=inet_addr("127.0.0.1");
        // Setup the port number to be any available port
        local.sin_port = htons(0);

        if(bind(temp_fd,(struct sockaddr*)&local,sizeof(local)) < 0){
                perror("Error with bind: bind()\n");
                exit(1);
        }

        if(listen(temp_fd,MAXCONNECTIONS)<0){
                perror("Error with listen: listen()\n");
                exit(1);
        }

        return temp_fd;
}

int main(int argc, char* argv[]){
        if(argc < 4){
                printf("Usage: %s <client_name> <server_ip> <server_port>",argv[0]);
                exit(1);
        } 

        int listenfd; // For incoming connections
        int connectfd; // Connect to the main server
        int remotesocketfd = -1; // Connect to the peer

        struct sockaddr_in servAddr;
        struct sockaddr_in remoteCliAddr, localAddr,tempSock; 
        socklen_t tempSockLen, remoteCliLen;

        memset(&servAddr,'0',sizeof(servAddr));
        memset(&remoteCliAddr,'0',sizeof(remoteCliAddr));
        memset(&localAddr,'0',sizeof(localAddr));
        memset(&tempSock,'0',sizeof(tempSock));
        memset(&tempSockLen,'0',sizeof(tempSockLen));

        tempSockLen = sizeof(tempSock);
        remoteCliLen = sizeof(remoteCliAddr);

        listenfd = setup_listen(localAddr);
        printf("Listen fd = %d\n",listenfd);

        char message[MAXMESGSIZE];
        memset(message,'\0',sizeof(message));
        int mesgLen = generate_message(message, listenfd,localAddr, argv[1]);

        // Register to the server:
        connectfd= socket(AF_INET, SOCK_STREAM,0);  
        if(connectfd <0){
                printf("Error in socket()\n");
                exit(1);
        }
        servAddr.sin_family = AF_INET;
        if(inet_pton(AF_INET, argv[2], &servAddr.sin_addr) <= 0){
                printf("inet_pton error!\n");
                exit(1);
        }
        servAddr.sin_port = htons((unsigned short)atoi(argv[3]));

        if(connect(connectfd,(struct sockaddr*)&servAddr,sizeof(servAddr)) < 0){
                        printf("Connect failed\n");
                        exit(1);
        }

        // Send the initial file list to the server:
        if(write(connectfd, message,mesgLen ) < 0){
                printf("Send error\n");
                exit(1);
        }
        else{
                printf("Intial message of %d bytes sent to server\n",strlen(message)); 
                printf("sent to %s %d\n",inet_ntoa(servAddr.sin_addr),ntohs(servAddr.sin_port)); 
        }
        
        fd_set read_fds;
        
        char sendMsg[MAXMESGSIZE];
        char recvBuff[MAXMESGSIZE];
        int recvBytes;

        struct record database[MAXNUMFILES];
        int dbIndex;

        int opt = 1;
        if( setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ){
                perror("setsockopt");
                exit(EXIT_FAILURE);
        }
        // Delay:
        sleep(2);

        while(true){
                FD_ZERO(&read_fds);
                FD_SET(listenfd,&read_fds);                        
                FD_SET(connectfd,&read_fds);
                // Add keyboard to the set:
                FD_SET(0,&read_fds);

                printf("---------------------------------------\n");
                printf("Client waiting\n");
        
                int retVal = select(FD_SETSIZE,&read_fds,NULL,NULL,NULL);
                if(retVal == -1){
                        perror("select()\n");
                        exit(1);
                }
                else if(retVal){
                        if(FD_ISSET(0,&read_fds)){
                                char buf[MAXCMDSIZE];
                                // Get the user input:
                                if (fgets(buf,MAXCMDSIZE,stdin)!= NULL){
                                        if(buf[strlen(buf)-1] == '\n'){
                                                buf[strlen(buf)-1] = 0;
                                        }
                                }
                                int bufLen = strlen(buf);
                                
                                if(strncmp(buf,"ls",2) == 0){
                                        // Show the current database:
                                        // Client always has the most recent database
                                        // as server pushes the changes automatically
                                        show_database(database,dbIndex);
                                }
                                else if (strncmp(buf,"exit",4) == 0){
                                        char test[5] ="exit\0";
                                        if(sendto(connectfd,test,strlen(test),0,
                                                        (struct sockaddr*)&servAddr,sizeof(servAddr)) < 0){
                                                perror("Send error() \n");
                                        }
                                        else{
                                                printf("sent to %s %d\n",inet_ntoa(servAddr.sin_addr),ntohs(servAddr.sin_port)); 
                                        }

                                        // Exit client cleanly:
                                        close(connectfd);
                                        close(listenfd);
                                        exit(0);

                                }
                                else if (strncmp(buf,"get",3) == 0){
                                        char* fileName;
                                        fileName = strndup(buf+4,bufLen);
                                        printf("Copyting file %s from peer\n",fileName);

                                        int fileIndex;
                                        fileIndex = get_file_owner_in_database(database, dbIndex, fileName);
                                        if(fileIndex > 0){
                                                printf("%s %s %s\n",database[fileIndex].name, 
                                                        database[fileIndex].fileOwner,database[fileIndex].ownerPort);
                                        }
                                        
                                        char getFileMsg[MAXMESGSIZE];
                                        memset(getFileMsg,'\0',sizeof(getFileMsg));
                                        strncat(getFileMsg,"get",3);
                                        strncat(getFileMsg,",",1);
                                        strncat(getFileMsg,fileName,strlen(fileName));
                                        getFileMsg[strlen(getFileMsg)]='\0';
                                        
                                        // Connect to peer:
                                        memset(&tempSock,'0',sizeof(tempSock));

                                        remotesocketfd= socket(AF_INET, SOCK_STREAM,0);  
                                        if(remotesocketfd <0){
                                                printf("Error in socket()\n");
                                                exit(1);
                                        }
                                        tempSock.sin_family = AF_INET;
                                        if(inet_pton(AF_INET,database[fileIndex].ownerIp,
                                                                &tempSock.sin_addr) <= 0){
                                                printf("inet_pton error!\n");
                                                exit(1);
                                        }
                                        tempSock.sin_port = htons((unsigned short)
                                                        atoi(database[fileIndex].ownerPort));

                                        if(connect(remotesocketfd,(struct sockaddr*)&tempSock,
                                                                sizeof(tempSock)) < 0){
                                                        printf("Connect failed\n");
                                                        exit(1);
                                        }
                                        printf("Connected to peer\n");

                                        if(sendto(remotesocketfd,getFileMsg,strlen(getFileMsg),0,
                                                        (struct sockaddr*)&tempSock,sizeof(tempSock)) < 0){
                                                perror("Send error() \n");
                                        }
                                        else{
                                                printf("sent to %s %d\n",inet_ntoa(tempSock.sin_addr),ntohs(tempSock.sin_port)); 
                                        }
                                        
                                        char fileBuffer[MAXBUFFSIZE];
                                        int fileBytes;
                                        memset(fileBuffer,'\0',sizeof(fileBuffer));
                                        if((fileBytes = recv(remotesocketfd,fileBuffer,MAXBUFFSIZE-1,0)) < 0){
                                                perror("Recv error\n");
                                                break;
                                        }
                                        fileBuffer[fileBytes]='\0';

                                        printf("Received file of size = %d bytes\n",fileBytes);

                                        //Write to the file:
                                        FILE* writefd;
                                        writefd =fopen(fileName,"wb");
                                        if(writefd == NULL){
                                                perror("fopen() error\n"); 
                                        }
                                        int writeBytes;
                                        writeBytes = fwrite(fileBuffer, sizeof(char),sizeof(fileBuffer),writefd);
                                        if(writeBytes = fileBytes){
                                                printf("Sucessfully written!\n"); 
                                        }
                                       
                                        fclose(writefd);
                                        close(remotesocketfd);

                                }                                 
                                memset(buf,'\0',MAXCMDSIZE);

                                continue;
                        } 
                        else if(FD_ISSET(listenfd,&read_fds)){
                                // Message from peer:
                                memset(&remoteCliAddr,'0',sizeof(remoteCliAddr));
                                memset(&remoteCliLen,'0',sizeof(remoteCliLen));
                                remoteCliLen = sizeof(remoteCliAddr);
                                int temp_fd = accept4(listenfd,(struct sockaddr*)&remoteCliAddr,
                                                &remoteCliLen,SOCK_NONBLOCK);
                                printf("listenfd = %d\n",listenfd);
                                if(temp_fd >0){
                                        memset(recvBuff,'\0',sizeof(recvBuff));
                                        if((recvBytes = read(temp_fd,recvBuff,MAXMESGSIZE-1)) < 0){
                                                perror("Recv error\n");
                                                break;
                                        }
                                        recvBuff[recvBytes]='\0';
                                        printf("Message %s from peer\n",recvBuff);

                                        if(strncmp(recvBuff,"get",3) ==0){
                                                char* file;
                                                char* buffer;
                                                file= strndup(recvBuff+4,strlen(recvBuff));

                                                // Open requested file:
                                                size_t fileSize;
                                                int fd;

                                                fd = open(file,O_RDONLY);
                                                if(fd <0){
                                                        printf("Cannot open file\n");
                                                        break;
                                                }
                                                fileSize = get_file_size(file);
                                                printf("File size = %d\n",fileSize);
                                                buffer =(char*) malloc(fileSize);
                                                if(buffer == NULL){
                                                        printf("Cannot allocate memory\n");
                                                        break;
                                                }
                                                memset(buffer,'\0',sizeof(buffer));

                                                size_t readBytes = read(fd, buffer,fileSize);
                                                printf("Read bytes = %d\n",readBytes);
                                                if(readBytes != fileSize){
                                                        printf("Read bytes not equal to fileSize\n"); 
                                                        break;
                                                }
                                                else{
                                                        if(sendto(temp_fd,buffer,fileSize,0,
                                                                (struct sockaddr*)&remoteCliAddr,sizeof(remoteCliAddr)) < 0){
                                                                perror("Send error() \n");
                                                        }
                                                        else{
                                                                printf("File of size %d sent to %s %d\n",fileSize,
                                                                inet_ntoa(remoteCliAddr.sin_addr),ntohs(remoteCliAddr.sin_port));
                                                                close(temp_fd);
                                                                                 
                                                        }
                                                }

                                        }

                                }
                        }
                        else if(FD_ISSET(connectfd,&read_fds)){
                                // Message from server: 
                                memset(recvBuff,'\0',sizeof(recvBuff));
                                if((recvBytes = read(connectfd,recvBuff,MAXMESGSIZE-1)) < 0){
                                        perror("Recv error\n");
                                        break;
                                }
                                recvBuff[recvBytes]='\0';

                                if(strncmp(recvBuff,"error",5) ==0){
                                        // Received error message from server:
                                        // Must be of the invalid name: Exit
                                        
                                        // Respond with exit message first:
                                        char sendErr[] ="quit\0"; 
                                        if(send(connectfd,sendErr,strlen(sendErr),0) < 0){
                                                perror("Send error\n");
                                                exit(1);
                                        }
                                        else{
                                                printf("%d bytes exit message sent to client\n",strlen(sendErr)); 
                                                // Client will respond with exit message
                                                // which gets handled later
                                        }

                                        printf("Server declined the name. Exiting\n");
                                        close(connectfd);
                                        close(listenfd);
                                        exit(0);
                                }
                                dbIndex = clear_database(database, dbIndex);
                                dbIndex = add_master_fileList_to_database(database,dbIndex,recvBuff);
                                continue;
                        }
                } // outerif
        } // while
        return 0;
}



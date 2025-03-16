#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <vector>
#include "opencv2/opencv.hpp"
#include <zlib.h>
#include <cstring>
using namespace std;
using namespace cv;
typedef struct {
    int length;
    int seqNumber;
    int ackNumber;
    int fin;
    int syn;
    int ack;
    int is_sent;
    unsigned long checksum;
} HEADER;

typedef struct{
    HEADER header;
    char data[1000];
} SEGMENT;

void setIP(char *dst, const char *src){
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 || strcmp(src, "localhost") == 0){
        sscanf("127.0.0.1", "%s", dst);
    }
    else{
        sscanf(src, "%s", dst);
    }
    return;
}

SEGMENT newAck(int ackNumber, int fin){
    SEGMENT seg;
    seg.header.ack = 1;
    seg.header.ackNumber = ackNumber;
    seg.header.fin = fin;
    return seg;
}

int main(int argc, char *argv[]){
    int sockfd, portNum, nBytes;
    float error_rate;
    SEGMENT s_tmp;
    struct sockaddr_in sender, agent, receiver, tmp_addr;
    socklen_t sender_size, recv_size, tmp_size;
    char sendIP[50], agentIP[50], recvIP[50], tmpIP[50];
    int sendPort, agentPort, recvPort;
    
    if(argc != 3){
        fprintf(stderr,"Usage: %s <receiver port> <agent IP>:<agent port>\n", argv[0]);
        exit(1);
    }
    else{
        sscanf(argv[1], "%d", &recvPort);               // receiver
        setIP(recvIP, "local");

        sscanf(argv[2], "%[^:]:%d", tmpIP, &agentPort);   // agent
        setIP(agentIP, tmpIP);
    }
    /* Create UDP socket */
    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    /* Configure settings in receiver struct */
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(recvPort);
    receiver.sin_addr.s_addr = inet_addr(recvIP);
    memset(receiver.sin_zero, '\0', sizeof(receiver.sin_zero)); 
    /* Configure settings in agent struct */
    agent.sin_family = AF_INET;
    agent.sin_port = htons(agentPort);
    agent.sin_addr.s_addr = inet_addr(agentIP);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));    
    /* bind socket */
    bind(sockfd,(struct sockaddr *)&receiver,sizeof(receiver));
    /* Initialize size variable to be used later on */
    recv_size = sizeof(receiver);
    tmp_size = sizeof(tmp_addr);
    int seqnum = 1;
    int acknum = 1;
    int buffer_size = 256;
    vector<SEGMENT> rcv_buffer;
    unsigned long checksum, prev_checksum;
    int height, width, image_size;
    int receive_size = 0;
    Mat imgClient;
    Mat frame;
    uchar *iptr;
    int recvSegSize, recvTotalSize;
    //SEGMENT seg;
    while(1){
        rcv_buffer.clear();
        while (rcv_buffer.size() < 257){
            memset(&s_tmp, 0, sizeof(s_tmp));
            bzero(s_tmp.data, 1000);
            if(recvfrom(sockfd, &s_tmp, sizeof(s_tmp), MSG_WAITALL, (struct sockaddr *)&tmp_addr, &tmp_size) < 0){    //MSG_WAITALL
                fprintf(stderr, "error in RecvSeg\n");
                exit(0);
            }
            if (s_tmp.header.seqNumber == seqnum){  
                checksum = crc32(0L, (const Bytef *)s_tmp.data, s_tmp.header.length);  
                if (checksum != s_tmp.header.checksum){     //corrupted
                    printf("drop\tdata\t#%d\t(corrupted)\n", s_tmp.header.seqNumber); //??                    
                    SEGMENT seg = newAck(seqnum-1, 0);
                    sendto(sockfd, &seg, sizeof(seg), 0, (struct sockaddr *)&agent, sizeof(agent));
                    printf("send\tack\t#%d\n", seqnum-1);
                }
                else{  
                    if (rcv_buffer.size() == 256){  //buffer overflow
                        if (s_tmp.header.fin == 1){
                            printf("drop\tfin\t\t(buffer overflow)\n");
                            SEGMENT seg = newAck(seqnum-1, 0);
                            sendto(sockfd, &seg, sizeof(seg), 0, (struct sockaddr *)&agent, sizeof(agent));
                            printf("send\tack\t#%d\n", seqnum-1);
                        }
                        else{
                            printf("drop\tdata\t#%d\t(buffer overflow)\n", s_tmp.header.seqNumber); //??
                            SEGMENT seg = newAck(seqnum-1, 0);
                            sendto(sockfd, &seg, sizeof(seg), 0, (struct sockaddr *)&agent, sizeof(agent));
                            printf("send\tack\t#%d\n", seqnum-1);
                            
                        }
                        break;
                    }
                    rcv_buffer.push_back(s_tmp);
                    if (s_tmp.header.fin == 0){
                        printf("recv\tdata\t#%d\n", s_tmp.header.seqNumber);
                        SEGMENT seg = newAck(seqnum, 0);
                        sendto(sockfd, &seg, sizeof(seg), 0, (struct sockaddr *)&agent, sizeof(agent));
                        printf("send\tack\t#%d\n", seqnum);
                        seqnum ++;
                    }
                    else{
                        printf("recv\tfin\n");
                        SEGMENT seg = newAck(seqnum, 1);
                        sendto(sockfd, &seg, sizeof(seg), 0, (struct sockaddr *)&agent, sizeof(agent));
                        seqnum ++;
                        printf("send\tfinack\n");
                        break;
                    }
                }
            }
            else{   // out of order
                if (s_tmp.header.fin == 1){
                    printf("drop\tfin\t\t(out of order)\n");
                    SEGMENT seg = newAck(seqnum-1, 0);
                    sendto(sockfd, &seg, sizeof(seg), 0, (struct sockaddr *)&agent, sizeof(agent));
                    printf("send\tack\t#%d\t", seqnum-1);
                    
                }
                else{
                    printf("drop\tdata\t#%d\t(out of order)\n", s_tmp.header.seqNumber); 
                    SEGMENT seg = newAck(seqnum-1, 0);
                    sendto(sockfd, &seg, sizeof(seg), 0, (struct sockaddr *)&agent, sizeof(agent));
                    printf("send\tack\t#%d\t\n", seqnum-1);
                    
                }
            }
        }
        printf("flush\n");
        for (int i=0; i<rcv_buffer.size(); i++){
            if(rcv_buffer[i].header.syn == 1){  //the first segment
                sscanf(rcv_buffer[i].data, "%d %d", &height, &width);   //read the resolution
                frame = Mat::zeros(height, width, CV_8UC3);
                if (!frame.isContinuous()){
                    frame = frame.clone();
                } 
                image_size = frame.total() * frame.elemSize();
                iptr = frame.data;
            }
            else if (rcv_buffer[i].header.fin == 1){  //end
                //char c = (char)waitKey(1000);
                destroyAllWindows();
                return 0;
            }
            else{
                memcpy(iptr+receive_size, (uchar*)rcv_buffer[i].data, rcv_buffer[i].header.length);
                receive_size += rcv_buffer[i].header.length;
                if (receive_size == image_size){
                    imshow("Video", frame); 
                    receive_size = 0;
                    char c = (char)waitKey(1000);
                }
            }
        }
    }



}

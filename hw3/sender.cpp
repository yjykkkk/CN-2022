#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <string>
#include <deque>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <zlib.h>
#include  <sys/types.h>
#include  <sys/times.h>
#include  <sys/select.h>

#include "opencv2/opencv.hpp"

#define MAXFD 20
using namespace std;
using namespace cv;

int windowThreshold;
int window_size;
int totalPutImageSize, putImageSize, video_is_end, send_all_video;
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

deque <SEGMENT> window;

SEGMENT newSeg(int seqNumber, int fin, int length, const char* data){
    SEGMENT seg;
    seg.header.ack = 0;
    seg.header.syn = 0;
    seg.header.seqNumber = seqNumber;
    seg.header.fin = fin;
    seg.header.length = length;
    seg.header.is_sent = 0;
    //memset(&seg.data, '\0', sizeof(char) * 1000);
    bzero(seg.data, 1000);
    memcpy(seg.data, data, length);
    seg.header.checksum = crc32(0L, (const Bytef *)seg.data, length);
    return seg;
}

void setIP(char *dst, const char *src){
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 || strcmp(src, "localhost") == 0){
        sscanf("127.0.0.1", "%s", dst);
    }
    else{
        sscanf(src, "%s", dst);
    }
    return;
}

void setWinSize(int timeout){
    if (timeout == 0){
        if(window_size < windowThreshold){
            window_size  *= 2;
        }
        else{
            window_size += 1;
        }
    }
    else{   //timeout
        windowThreshold = max(window_size/2, 1);
        window_size = 1;
    }
}

VideoCapture cap;

int main(int argc, char *argv[]){
    int sockfd, portNum, nBytes;
    SEGMENT s_tmp;
    struct sockaddr_in sender, agent, receiver, tmp_addr;
    socklen_t sender_size, recv_size, tmp_size;
    char sendIP[50], agentIP[50], recvIP[50], tmpIP[50];
    int sendPort, agentPort, recvPort;
    string filename;
    
    if(argc != 4){
        fprintf(stderr,"Usage: %s <sender port> <agent IP>:<agent port> <filename>\n", argv[0]);
        exit(1);
    }
    else{
        sscanf(argv[1], "%d", &sendPort);               // receiver
        setIP(sendIP, "local");

        sscanf(argv[2], "%[^:]:%d", tmpIP, &agentPort);   // agent
        setIP(agentIP, tmpIP);

        filename = string(argv[3]);
    }    
    /* Create UDP socket */
    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    /* Configure settings in sender struct */
    sender.sin_family = AF_INET;
    sender.sin_port = htons(sendPort);
    sender.sin_addr.s_addr = inet_addr(sendIP);
    memset(sender.sin_zero, '\0', sizeof(sender.sin_zero));  
    /* Configure settings in agent struct */
    agent.sin_family = AF_INET;
    agent.sin_port = htons(agentPort);
    agent.sin_addr.s_addr = inet_addr(agentIP);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));    
    /* bind socket */
    bind(sockfd,(struct sockaddr *)&sender,sizeof(sender));
    /* Initialize size variable to be used later on */
    sender_size = sizeof(sender);
    tmp_size = sizeof(tmp_addr);

    fd_set read_set, read_set_copy;//因為select會改掉set，所以用之前要先複製一份，不要直接把原本的拿去用
    FD_ZERO(&read_set);
    windowThreshold = 16;
    window_size = 1;
    int seqnum = 1;
    cap.open(filename.c_str());
    int width = cap.get(CAP_PROP_FRAME_WIDTH);
    int height = cap.get(CAP_PROP_FRAME_HEIGHT);
    Mat imgServer = Mat::zeros(height, width, CV_8UC3);      
    int image_size = imgServer.total() * imgServer.elemSize();
    if (!imgServer.isContinuous()){
        imgServer = imgServer.clone();
    }
    char resolution[1000];
    sprintf(resolution, "%d %d", height, width);
    SEGMENT info = newSeg(seqnum, 0, strlen(resolution), resolution); 
    seqnum ++;
    info.header.syn = 1;
    int send_num = sendto(sockfd, &info, sizeof(info), 0, (struct sockaddr *)&agent, sizeof(agent));
    printf("send\tdata\t#%d,\twinSize = %d\n", info.header.seqNumber, window_size);
    int select_num, sent_num;
    FD_SET(sockfd, &read_set);
    struct timeval tv;
    while (1){
        memcpy(&read_set_copy, &read_set, sizeof(read_set));
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        select_num = select(MAXFD, &read_set_copy, NULL, NULL, &tv);
        if (select_num == -1){
            continue;
        }
        else if (select_num == 0){  //time out
            setWinSize(1);  
            printf("time\tout,\t\tthreshold = %d\n", windowThreshold);
            sendto(sockfd, &info, sizeof(info), 0, (struct sockaddr *)&agent, sizeof(agent));
            printf("resnd\tdata\t#%d,\twinSize = %d\n", info.header.seqNumber, window_size);
        }
        else{
            SEGMENT rcv_seg;
            recvfrom(sockfd, &rcv_seg, sizeof(rcv_seg), 0, (struct sockaddr *)&tmp_addr, &tmp_size);
            printf("recv\tack\t#%d\n", rcv_seg.header.ackNumber);
            if (rcv_seg.header.ackNumber != 1){
                sendto(sockfd, &info, sizeof(info), 0, (struct sockaddr *)&agent, sizeof(agent));
                printf("resnd\tdata\t#%d,\twinSize = %d\n", info.header.seqNumber, window_size);
            }
            else{
                setWinSize(0);
                break;
            }
        }
    }
    video_is_end = 0;   //if video_is_end = 1, means finishing putting all video frame into window
    send_all_video = 0; //if send_all_video = 1, means finishing sending the whole video 
    cap >> imgServer;
    if (imgServer.empty()){ //actually won't enter this if
        newSeg(seqnum, 1, 0, NULL);
        video_is_end = 1;
    }
    totalPutImageSize = 0;
    while (1){
        while(window.size() < window_size && !video_is_end){
            if(totalPutImageSize + 1000 < image_size) {
                window.push_back(newSeg(seqnum, 0, 1000, (const char*)imgServer.data+totalPutImageSize));
                seqnum ++;
                totalPutImageSize += 1000;
            }
            else {
                window.push_back(newSeg(seqnum, 0, image_size-totalPutImageSize, (const char*)imgServer.data+totalPutImageSize));
                seqnum ++;
                cap >> imgServer;
                totalPutImageSize = 0;
                if (imgServer.empty()){
                    window.push_back(newSeg(seqnum, 1, 0, NULL));   //push fin
                    seqnum++;  
                    video_is_end = 1;
                    break;
                }
            }
        }
        int wannaSend;
        if (window.size() != 0){
            wannaSend = min((int)window.size(), window_size);
            for (int i=0; i<wannaSend; i++){
                if (window[i].header.is_sent == 0){
                    sendto(sockfd, &window[i], sizeof(window[i]), 0, (struct sockaddr *)&agent, sizeof(agent));
                    window[i].header.is_sent = 1;
                    if (window[i].header.fin == 0){
                        printf("send\tdata\t#%d,\twinSize = %d\n", window[i].header.seqNumber, window_size);
                    }
                    else{
                        printf("send\tfin\n");
                    }
                }
                else{
                    sendto(sockfd, &window[i], sizeof(window[i]), 0, (struct sockaddr *)&agent, sizeof(agent));
                    if (window[i].header.fin == 0){
                        printf("resnd\tdata\t#%d,\twinSize = %d\n", window[i].header.seqNumber, window_size);
                    }
                    else{
                        printf("resnd\tfin\n");
                    }
                }
            }
        }
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int correctAckedNum = 0;
        int isTimeOut = 0;
        while (correctAckedNum < window_size && !window.empty()){
            memcpy(&read_set_copy, &read_set, sizeof(read_set));
            select_num = select(MAXFD, &read_set_copy, NULL, NULL, &tv);    //timeout 會被自動更新
            if (select_num == -1){
                continue;
            }
            else if (select_num == 0){  //timeout
                setWinSize(1);
                isTimeOut = 1;
                printf("time\tout,\t\tthreshold = %d\n", windowThreshold);
                break; 
            }
            else{
                isTimeOut = 0;
                SEGMENT rcv_seg;
                recvfrom(sockfd, &rcv_seg, sizeof(rcv_seg), MSG_WAITALL, (struct sockaddr *)&tmp_addr, &tmp_size);            
                if (rcv_seg.header.fin == 0){
                    printf("recv\tack\t#%d\n", rcv_seg.header.ackNumber);                
                    if (rcv_seg.header.ackNumber == window[0].header.seqNumber){
                        window.pop_front();
                        correctAckedNum += 1;
                        tv.tv_sec = 1;
                        tv.tv_usec = 0;
                    }
                    else if (rcv_seg.header.ackNumber > window[0].header.seqNumber){    //收到 out-of-order ACK (實際上還沒收到前面的ack)
                        while(window[0].header.seqNumber <= rcv_seg.header.ackNumber){  //cumulative ack
                            window.pop_front();
                            correctAckedNum += 1;
                            if (window.empty()){
                                break;
                            }
                        }
                        tv.tv_sec = 1;
                        tv.tv_usec = 0;
                    }
                    else{   //receive unexpecting ack
                        //do nothing
                    }
                }
                else{   //fin
                    printf("recv\tfinack\n");   
                    correctAckedNum += 1;
                    //video_is_end = 1;
                    send_all_video = 1;
                    //window.pop_front(); //doesn't really matter
                    break;
                }
            }
        }
        if (!isTimeOut)
            setWinSize(0);
        if (send_all_video){
            break;
        }
    }
    cap.release();
}

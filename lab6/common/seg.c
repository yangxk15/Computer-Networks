#include "seg.h"

//send function
int sendf(sendseg_arg_t *ptr, int connection)
{
	ptr->seg.header.checksum = checksum(&ptr->seg);
	char bufstart[2];
	char bufend[2];
	bufstart[0] = '!';
	bufstart[1] = '&';
	bufend[0] = '!';
	bufend[1] = '#';
	
	static pthread_mutex_t* lock = NULL;
	if (lock == NULL)
	{
		lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(lock, NULL);
	}
	pthread_mutex_lock(lock);
	if (send(connection, bufstart, 2, 0) < 0) {
		pthread_mutex_unlock(lock);
		return -1;
	}
	int length = sizeof(ptr->nodeID) + sizeof(ptr->seg.header) + ptr->seg.header.length;
	if(send(connection, ptr, length, 0) < 0) {
		pthread_mutex_unlock(lock);
		return -1;
	}
	if(send(connection, bufend, 2, 0) < 0) {
		pthread_mutex_unlock(lock);
		return -1;
	}
	pthread_mutex_unlock(lock);
		
	return 1;
}

//recv function
int recvf(sendseg_arg_t *ptr, int connection)
{
	char buf[sizeof(sendseg_arg_t) + 2]; 
	char c;
	int idx = 0;
	// state can be 0,1,2,3; 
	// 0 starting point 
	// 1 '!' received
	// 2 '&' received, start receiving segment
	// 3 '!' received,
	// 4 '#' received, finish receiving segment 
	int state = 0; 
	while(recv(connection,&c,1,0)>0) {
		if (state == 0) {
		        if(c=='!')
				state = 1;
		}
		else if(state == 1) {
			if(c=='&') 
				state = 2;
			else
				state = 0;
		}
		else if(state == 2) {
			if(c=='!') {
				buf[idx]=c;
				idx++;
				state = 3;
			}
			else {
				buf[idx]=c;
				idx++;
			}
		}
		else if(state == 3) {
			if(c=='#') {
				buf[idx]=c;
				memcpy(ptr, buf, idx - 1);
				state = 0;
				idx = 0;

				//add segment error	
				if(seglost(&ptr->seg)>0) {
					continue;	
			    }

				if(checkchecksum(&ptr->seg)<0) {
					printf("checksum error,drop!\n");
					continue;
				}
				return 1;
			}
			else if(c=='!') {
				buf[idx]=c;
				idx++;
			}
			else {
				buf[idx]=c;
				idx++;
				state = 2;
			}
		}
	}
	return -1;
}

//SRT process uses this function to send a segment and its destination node ID in a sendseg_arg_t structure to SNP process to send out. 
//Parameter network_conn is the TCP descriptor of the connection between the SRT process and the SNP process. 
//Return 1 if a sendseg_arg_t is succefully sent, otherwise return -1.
int snp_sendseg(int network_conn, int dest_nodeID, seg_t* segPtr)
{
	sendseg_arg_t a;
	a.nodeID = dest_nodeID;
	memcpy(&a.seg, segPtr, sizeof(seg_t));
	return sendf(&a, network_conn);
}

//SRT process uses this function to receive a  sendseg_arg_t structure which contains a segment and its src node ID from the SNP process. 
//Parameter network_conn is the TCP descriptor of the connection between the SRT process and the SNP process. 
//When a segment is received, use seglost to determine if the segment should be discarded, also check the checksum.  
//Return 1 if a sendseg_arg_t is succefully received, otherwise return -1.
int snp_recvseg(int network_conn, int* src_nodeID, seg_t* segPtr)
{
	sendseg_arg_t a;
	if (-1 == recvf(&a, network_conn)) {
		return -1;
	}
	*src_nodeID = a.nodeID;
	memcpy(segPtr, &a.seg, sizeof(seg_t));
	return 1;
}

//SNP process uses this function to receive a sendseg_arg_t structure which contains a segment and its destination node ID from the SRT process.
//Parameter tran_conn is the TCP descriptor of the connection between the SRT process and the SNP process. 
//Return 1 if a sendseg_arg_t is succefully received, otherwise return -1.
int getsegToSend(int tran_conn, int* dest_nodeID, seg_t* segPtr)
{
	sendseg_arg_t a;
	if (-1 == recvf(&a, tran_conn)) {
		return -1;
	}
	*dest_nodeID = a.nodeID;
	memcpy(segPtr, &a.seg, sizeof(seg_t));
	return 1;
}

//SNP process uses this function to send a sendseg_arg_t structure which contains a segment and its src node ID to the SRT process.
//Parameter tran_conn is the TCP descriptor of the connection between the SRT process and the SNP process. 
//Return 1 if a sendseg_arg_t is succefully sent, otherwise return -1.
int forwardsegToSRT(int tran_conn, int src_nodeID, seg_t* segPtr)
{
	sendseg_arg_t a;
	a.nodeID = src_nodeID;
	memcpy(&a.seg, segPtr, sizeof(seg_t));
	return sendf(&a, tran_conn);
}

//lost rate is PKT_LOSS_RATE defined in constant.h
//if a segment has is lost, return 1; otherwise return 0 
//PKT_LOSS_RATE/2 probability of segment loss
//PKT_LOSS_RATE/2 probability of invalid checksum
//
// Pseudocode
// 1) Randomly decide whether to mess with this segment
// 2) If so, flip a coin, if heads seglost, if tails
//    flip a bit and return the result
//
int seglost(seg_t* segPtr) {
	int random = rand()%100;
	if(random<PKT_LOSS_RATE*100) {
		//50% probability of losing a segment
		if(rand()%2==0) {
			printf("seg lost!!!\n");
                        return 1;
		}
		//50% chance of invalid checksum
		else {
			//get data length
			int len = sizeof(srt_hdr_t)+segPtr->header.length;
			//get a random bit that will be fliped
			int errorbit = rand()%(len*8);
			//flip the bit
			char* temp = (char*)segPtr;
			temp = temp + errorbit/8;
			*temp = *temp^(1<<(errorbit%8));
			return 0;
		}
	}
	return 0;

}



//check checksum
//Denote the data the checksum is calculated as D
//D = segment header + segment data
//if size of D (in bytes) is odd number
//append an byte with all bits set as 0 to D
//Divide D into 16-bits-long values
//add all these 16-bits-long values using 1s complement
//the sum should be FFFF if it's valid checksum
//so flip all the bits of the sum 
//return 1 if the result is 0 
//return -1 if the result is not 0 
//
// Pseudocode
// 1) sum = 0
// 2) Get number of 16-bit blocks in segment (len)
//      increment by 1 if need to
// 3) temp = segment (temp is a pointer)
// 4) while(len > 0)
//      sum += *temp
//      temp++
//      if(sum & 0x10000)
//        sum = (sum & 0xFFFF)+1 // Check for and take care of overflow
//      len--
// 5) result = ~sum
// 6) if(resul == 0) return 1
//    else return -1
//
int checkchecksum(seg_t *segment){
        long sum = 0;
        //len is the number of 16-bit data to calculate the checksum
	if (segment->header.length < 0 || segment->header.length > MAX_SEG_LEN)
		return -1;
        int len = sizeof(srt_hdr_t)+segment->header.length;
        if(len%2==1)
        	len++;
        len = len/2;
        unsigned short* temp = (unsigned short*)segment;

        while(len > 0){
        	sum += *temp;
       		temp++;
        	//if overflow, round the most significant 1
        	if(sum & 0x10000)
        		sum = (sum & 0xFFFF) + 1;
        	len --;
       }
        
       unsigned short result =~sum;
       if(result == 0)
		return 1;
	else
		return -1;
}


//calculate checksum over the segment
//clear the checksum field in segment to be 0
//Denote the data from which the checksum is calculated as D
//D = segment header + segment data
//if size of D (in bytes) is odd number
//append a byte with all bits set as 0 to D
//Divide D into 16-bits-long values
//add all these 16-bits-long values using 1s complement
//flip the all the bits of the sum to get the checksum
unsigned short checksum(seg_t *segment){
	segment->header.checksum = 0;
	if(segment->header.length%2==1)
		segment->data[segment->header.length] = 0;
	
	long sum = 0;  
	//len is the number of 16-bit data to calculate the checksum
	int len = sizeof(srt_hdr_t)+segment->header.length;
	if(len%2==1)
		len++;
	len = len/2;
	unsigned short* temp = (unsigned short*)segment;

        while(len > 0){
        	sum += *temp;
		temp++;
		//if overflow, round the most significant 1
             	if(sum & 0x10000)   
               		sum = (sum & 0xFFFF) + 1;
             	len --;
        }
           	
	return ~sum;
}

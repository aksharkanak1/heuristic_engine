#ifndef __HEURISTIC_H__
#define __HEURISTIC_H__
#include "asm/unistd.h"   // for system call numbers 

/*Defines*/


#define HE_MAX_FILE_NAME_SIZE                       256
#define HE_MAX_MONITORED_FILES_SUPPORTED            100
#define HE_MAX_BLACK_LISTED_IPS_SUPPORTED           100
#define HE_CONFIG_FILE_HASH_TABLE_MAX_SIZE                  20 
#define HE_CONFIG_BL_IP_HASH_TABLE_MAX_SIZE                  20 
#define HE_CONFIG_MAX_OPS			    500 /*This should be same as that of number
                                                          of system calls */	
#define HE_CONFIG_TAG_MAX_SIZE                      100 //Max size of the tag <tag>
#define HE_CONFIG_TAG_DATA_LINE_MAX_SIZE            250 /* Max size of each line 
                                                         after a tag */
#define HE_CONFIG_OP_MAX_SIZE                       (HE_CONFIG_TAG_DATA_LINE_MAX_SIZE-4) /*considering :<score>*/


#define HE_CONFIG_CHILD_ROOT_FILE_NAME_MAX_SIZE      HE_CONFIG_TAG_DATA_LINE_MAX_SIZE

#define HE_FILE_MONITERED        0x01
#define HE_FILE_NOT_MONITERED    0x02


#define HE_SOCKET_TYPE_TCP       0x01
#define HE_SOCKET_TYPE_UDP       0x02
#define HE_SOCKET_CONNECTED      0x03 

#define HE_OP_SUCCESS            0x01
#define HE_OP_FAILURE            0x02

#define HE_MAX_FD_PER_PROCESS    1024
#define HE_MAX_SD_PER_PROCESS    1024

/*he child context file descriptor table flags */
#define HE_CHILD_CTX_FILE_OPEN_FLAG            0x01
#define HE_CHILD_CTX_FILE_READ_FLAG            0x02
#define HE_CHILD_CTX_FILE_WRITE_FLAG           0x04
#define HE_CHILD_CTX_FILE_CLOSE_FLAG           0x08

/*he child context socket descriptor table flags */
#define HE_CHILD_CTX_SOCKET_CREATION_FLAG      0x01
#define HE_CHILD_CTX_SOCKET_CONNECT_FLAG       0x02
#define HE_CHILD_CTX_SOCKET_READ_FLAG          0x04
#define HE_CHILD_CTX_SOCKET_WRITE_FLAG         0x08
#define HE_CHILD_CTX_SOCKET_CLOSE_FLAG         0x10





#define HE_CONFIG_OP_ON_MON_FILE     0x01
#define HE_CONFIG_OP_ON_BL_IP        0x02
#define HE_CONFIG_OP_NORMAL          0x04   

#define HE_CONFIG_FILE            "./he_config.cfg"

#define HE_CONFIG_DELIMITER       ':'
#define HE_CONFIG_MAX_OP_TO_ID_MAPPING 17

#define HE_FLAG_FOR_TAG_PARSE_FUNC_TAG_START     0x01
#define HE_FLAG_FOR_TAG_PARSE_FUNC_TAG_END       0x02   

#define HE_FD_TYPE_IS_FILE       0x01
#define HE_FD_TYPE_IS_SOCKET     0x02
#define HE_FD_TYPE_IS_NONE       0x00 

#define HE_FILE_OPEN_SUCCESS      0x01
#define HE_FILE_OPEN_FAILURE      0x02
#define HE_FILE_CLOSE_SUCCESS     0x04
#define HE_FLE_CLOSE_FAILURE      0x08

#define HE_SOCK_CONNECT_SUCCESS      0x01
#define HE_SOCK_CONNECT_FAILURE      0x02
#define HE_SOCK_CLOSE_SUCCESS        0x04
#define HE_SOCK_CLOSE_FAILURE        0x08


#define HE_CLEAR_NEW_LINE_FROM_STR(line)     if(line[strlen(line)-1] == '\n') \
                                                 line[strlen(line)-1] = '\0'; 



#define HE_SYSTEM_CALL_SUCCESS_STATUS(ret)  (ret>=0 : 1? 0 )

#define IS_SYS_CALL_READ_OR_WRITE_OP(num,subOp)  ((num == __NR_write) || (num == __NR_read) ||     \
                                                ((num == __NR_socketcall) && (subOp == SYS_SEND || subOp ==SYS_SENDTO || \
                                                subOp == SYS_SENDMSG || subOP == SYS_RECV ||subOp ==SYS_RECVFROM || \ 
                                                (subOp == SYS_RECVMSG))))
#define IS_SYS_CALL_FILE_OPEN(num)  (num == __NR_open)
#define IS_SYS_CALL_SOCKET_TYPE(pRegs)  (__NR_socketcall == pRegs->orig_eax)
#define IS_SOCKET_CALL_VALID_OP(pRegs)  ((pRegs->ebx == SYS_SEND || pRegs->ebx ==SYS_SENDTO || \
                                         pRegs->ebx == SYS_SENDMSG || pRegs->ebx == SYS_RECV ||  \
                                         pRegs->ebx ==SYS_RECVFROM || subOp == SYS_RECVMSG)) 
#define IS_SOCKET_CALL_TYPE_CONNECT(pRegs)  (pRegs->ebx == SYS_CONNECT)                                                

#define HE_FILE_OP_SYS_CALLNUM_CONVERT(sysNum,ptrConNum )       \
 {   \
     switch (sysNum )   \
     {                   \
         case __NR_open :                                 \
             *ptrConNum = HE_CHILD_CTX_FILE_OPEN_FLAG ;    \
              break;                                       \
         case __NR_read :                                 \
              *ptrConNum = HE_CHILD_CTX_FILE_READ_FLAG ;    \
              break;                                        \
         case __NR_write :                                   \
              *ptrConNum = HE_CHILD_CTX_FILE_WRITE_FLAG ;   \
               break;                                       \
         default :                                         \
              printf("(fn %s) Invalid sys cal number for error",__func__);  \                                         return -1;                                     \ 
      }              \
 } 


#define HE_OP_HAS_BEEN_SUCCESSFUL(ret)  ((ret <= 0) ?HE_OP_FAILURE :HE_OP_SUCCESS )

/*enums*/


typedef enum {
     HE_CONFIG_TAG_ROOT_FOR_CHILD   =   0x00
     HE_CONFIG_TAG_BLACK_LISTED_IP ,   
     HE_CONFIG_TAG_BLACK_LISTED_PORTS , 
     HE_CONFIG_TAG_FILES_TO_MONITER,
     HE_CONFIG_TAG_TRACK_OP_ON_MON_FILE,
     HE_CONFIG_TAG_TRACK_OP_ON_BL_IP_PORT,
     HE_CONFIG_TAG_TRACK_OP ,
     HE_CONFIG_TAG_THRESHOLD,
     HE_CONIG_MAX_NUMBER_OF_TAGS 
}HE_CONFIG_TAG_IDX ;


typedef enum {
}HE_OP_NUMBERS ; 


/*Strcutures*/
typedef struct  he_simple_node{
   struct he_simple_node *next ;
   struct he_simple_node *prev;

}he_simple_node;


typedef struct he_file_table_entry
{
   struct he_file_table_entry *next ;
   struct he_file_table_entry *prev ;
   uint8_t fname[HE_MAX_FILE_NAME_SIZE+1];
   uint32_t flag ;
   uint8_t isOpenCloseSuccess ;
   uint32_t dataRead ;
   uint32_t dataWritten;
   uint32_t numWriteFailure;
   uint32_t numWriteSuccess;
   uint16_t numReadFailure ;
   uint16_t numReadSuccess;
}he_file_table_entry ;

typedef struct he_ChildCtxFileTableBucket 
{
    he_file_table_entry *next ;
    he_file_table_entry *last;
}he_ChildCtxFileTableBucket ;

typedef struct he_sock_table_entry
{
   struct he_sock_table_entry *next ;
   struct he_sock_table_entry *prev ;
   uint8_t isConCloseSuccess;
   uint32_t ip ;
   uint16_t port;
   uint32_t flag ;
   uint32_t dataRead ;
   uint32_t dataWritten;
   uint32_t numWriteFailure;
   uint32_t numWriteSuccess;
   uint16_t numReadFailure ;
   uint16_t numReadSuccess;
}he_sock_table_entry ;

typedef struct he_ChildCtxSockTableBucket 
{
    he_sock_table_entry *next ;
    he_sock_table_entry *last;
}he_ChildCtxSockTableBucket ;


/*This structure has to be enhanced to include even the arguments of the system calls*/
typedef struct he_child_op_list
{ 
   struct he_child_op_list *next ;
   struct he_child_op_list *prev ;
   uint32_t op ;  // will be containg the system call number
   uint32_t subOp;// might contain the sub op , like in case of socketcall 
   uint8_t result ;
}he_child_op_list ;

typedef struct he_child_ctx 
{
    he_ChildCtxFileTableBucket *pfileTable ;
    he_ChildCtxiSockTableBucket  *psockTable;
    he_child_op_list *pChildops;
    uint32_t score ;
}he_child_ctx;

/*Components of config structure*/

typedef struct he_MonFileEntry {
  struct he_MonFileEntry *next ;
  struct he_MonFileEntry *prev;
  uint8_t file[HE_MAX_FILE_NAME_SIZE+1];  
}he_MonFileEntry ;

typedef struct he_MonFileEntry {
  struct he_MonFileEntry *next ;
  struct he_MonFileEntry *prev;
  uint32_t ip;  
}he_BLIpEntry ;

typedef struct he_ops
{
   struct he_ops *next ;
   struct he_ops *prev;
   uint32_t score ;
   uint32_t opScope; /*refers to if the operation is a normal operation or is it operation on monitored files 
                       or its a operation to monitor black listed ip */
   uint32_t flags ; /*provides infomration like if its a suboperation .Like for example socketcall is the main system call
                    and soecket, connect , bind are the suboperation*/ 
   uint32_t subOp ; /*valid only if HE_ENG_OP_FLAG_SUB_OP is set in flags*/
   void *ptr ;  /*might be used for some specific data*/  
}he_ops;

typedef struct he_Config 
{
     char rootForChild[HE_CONFIG_CHILD_ROOT_FILE_NAME_MAX_SIZE] ;
     uint32_t numMonFiles;
     uint32_t numBLIp ;
     he_MonFileEntry *pMonFileTable[HE_CONFIG_FILE_HASH_TABLE_MAX_SIZE];
     he_BLIpEntry *pBLIPEntry[HE_CONFIG_BL_IP_HASH_TABLE_MAX_SIZE];
     he_ops *pOps[HE_CONFIG_MAX_OPS];
     uint32_t thrScore;
}he_Config ;

typedef struct he_tagParseFnStruct
{
     char startTag[HE_CONFIG_TAG_MAX_SIZE+1];  
     char endTag[HE_CONFIG_TAG_MAX_SIZE+1];
     CfgParsingfn parseFn ;
}he_tagParseFnStruct;

/*The below structure is used to maintain a mapp between the operation names and system call number */
/*some of the system calls use wrappers so we need suOp also  */
typedef struct he_OPToNumMapping{
  char opStr[HE_CONFIG_OP_MAX_SIZE];
  uint32_t op ;
  uint32_t subOp; 
}he_OPToNumMapping;


typedef void* (*malloc_callbk)(uint32_t size);

/*Call back function for confiuration parsing*/
typedef int (*CfgParsingfn)(int fd ,he_Config * , char *StartTag ,char *EndTag ,uint32_t flag); 

#endif 

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <ctype.h>
#include <pwd.h>
#include "linux/net.h"
#include "linux/ipc.h"
#include "heuristic_main.h"

/*Structure for storing the parse function for the tags */
he_tagParseFnStruct  gTagParseFnTable[HE_CONIG_MAX_NUMBER_OF_TAGS] ;
extern he_Config gCfg={0,};

/*Structure for storing op nmaes to id mapping . The id is same as that of the system call numbers
the ids are taken from /usr/include/asm/unistd_32.h */

/*the action or num required by socketcall are in the file linux/net.h*/
/*The call number required for ipc is in file /usr/include/linux/ipc.h*/
/*Tracking shared memory attach should be sufficient and no need to track the shared memeory creation 
since after creation of the shared memory attach should be done to use it */
he_OPToNumMapping  gOpToNumMap[HE_CONFIG_MAX_OP_TO_ID_MAPPING] =  {
          {"FILE_OPEN",__NR_open,0},
          {"FILE_READ",__NR_read,0},
          {"FILE_CHOWN",__NR_chown,0},
          {"FILE_DELETE",_NR_unlink,0},
          {"FILE_CHMOD",___NR_chmod},0,
          {"FILE_WRITE",__NR_write,0},
          {"SET_UID",__NR_setuid,0},
          {"CHILD_CREATION",__NR_fork,0},
          {"CONNECT_TO_BLACKLIST_IP",__NR_socketcall,SYS_CONNECT},
          {"WRITING_TO_BLACK_LISTIP",__NR_write,0},
          {"SEND_TO_BLACKLIST_IP",__NR_socketcall,SYS_SEND},
          {"SENDTO_TO_BLACK_LISTIP",__NR_socketcall,SYS_SENDTO},
          {"SENDMSG_TO_BLACK_LISTIP",__NR_socketcall,SYS_SENDMSG},
          {"READING_FROM_BLACK_LIST_IP",__NR_read,0},
          {"RECV_FROM_BLACK_LIST_IP",__NR_socketcall,SYS_RECV},
          {"RECVFROM_FROM_BLACK_LIST_IP",__NR_socketcall,SYS_RECVFROM},
          {"RECVMSG_FROM_BLACK_LIST_IP",__NR_socketcall,SYS_RECVMSG}
        
};

int he_GetHashIndexForGivenString(uint8_t *str, uint32_t len , uint32_t hashtableLen )
{
   int i =0;
   uint32_t total =0; 
   uint32_t idx =0;
   if(NULL == str )
   {
       return -1 ;
   }

  for(i=0;i<len;i++)
  {
      total +=str[i]; 
  }
  
  idx = total % hashtableLen;
  return idx ;
   
}

/*
    API for gettting the hash table index or bucket for a given ip address (as key)
    the hash function used is jenkins hash function , The below code is taken from  
    jenkins has table wikipedia article
*/

uint32_t   he_GetHashIndexGoGivenIP(uint32_t key , uint32_t len , uint32_t hastableLen)
{
    uint32_t hash, i;
    for(hash = i = 0; i < len; ++i)
    {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return (hash % hastableLen);
}


void* he_getMem(uint32_t size)
{
    void * temp ;
 
    temp =  malloc (size);
    if(NULL == temp)
    {
       return -1 ;
    } 
    memset((uint8_t*)temp , 0 , size);
    return temp;
}


he_simple_node * he_get_Node(he_simple_node ** pNode , uint32_t size  )
{
     he_simple_node *temp =NULL ;
     if(NULL  ==  pNode )
     {
           return NULL  ;
   
     } 
     if(NULL == *pNode)
     {
         *pNode = (he_simple_node*)he_getMem(size);
         return *pNode ; 
     }
    
     temp = *pNode ;
     while (temp->next != NULL)
          temp =  temp->next ;
    
     temp->next = (he_simple_node*)he_getMem(size);
     if(NULL != temp->next )
         temp->next->prev = temp;
     else 
         return NULL ; 
     
     return temp->next ;
     
}


/*
    API for updating the file table entry in the child context . 
*/

int he_update_chd_fd_table(int fd , he_child_ctx *pChdCtx, uint32_t flags,void *data,uint8_t start,uint8_t opStatus)
{
    he_file_table_entry** temp = NULL;
    he_file_table_entry* temp1 = NULL;
    char * file = NULL;
    uint8_t newNode =0;

    uint32_t sizeTocopy ;
    if(fd < 0 || fd > HE_MAX_FD_PER_PROCESS || NULL == pChdCtx  )
    {
        return -1 ;
    }

    if(NULL == pChdCtx->pfileTable)
    { 
        pChdCtx->pfileTable =  malloc (HE_MAX_FD_PER_PROCESS* sizeof(he_ChildCtxFileTableBucket));
        if(NULL  == pChdCtx->pfileTable)
        {
           return -1 ;			
        }
        memset(pChdCtx->pfileTable, 0,sizeof(HE_MAX_FD_PER_PROCESS* sizeof(he_ChildCtxFileTableBucket)));
      
    }
  
    /*if the file is being opened then only allocate the file table entry from the 
     file table bucket*/
    if(flags == HE_CHILD_CTX_FILE_OPEN_FLAG && HE_SYSTEM_CALL_BEGINING == start)
    { 
        temp = &((pChdCtx->pfileTable)[fd].first);
        temp1 = (he_file_table_entry*)he_get_Node((he_simple_node **)temp,sizeof(he_file_table_entry));
        newNode =1;
    }
    else 
    {
       /*Take the last node in the list , because it will be the file table entry which is active for that fd*/
       temp1 = (pChdCtx->pfileTable)[fd]->last;
    } 

    if(NULL == temp1)
    {
       printf("(fn %s) Failed to get the node for he_file_table_entry",__func__);
       return -1 ;
    }
   
    switch(flags)
    {
        case HE_CHILD_CTX_FILE_OPEN_FLAG :
             if(HE_SYSTEM_CALL_BEGINING == start)
             {
                file = (char *)data; 
                sizeTocopy = (strlen(file) >HE_MAX_FILE_NAME_SIZE)?HE_MAX_FILE_NAME_SIZE :strlen(file);
                memcpy(temp1->fname, file , sizeTocopy);
                temp1->fname[sizetocopy] = '\0';
             }
             else 
             {
                temp1->isOpenCloseSuccess |= (opStatus == HE_OP_SUCCESS)?HE_FILE_OPEN_SUCCESS :HE_FILE_OPEN_FAILURE ;
             } 
             break;
        case HE_CHILD_CTX_FILE_READ_FLAG :
            if(HE_OP_SUCCESS == opStatus)
            {
              (temp1->dataRead)+= *((uint32_t*)(data));
              temp1->numReadSuccess++;
            } 
            else 
              temp1->numReadFailure++;
            break;
        case HE_CHILD_CTX_FILE_WRITE_FLAG :
            if(HE_OP_SUCCESS == opStatus)
            {
                (temp1->dataWritten)+= *((uint32_t*)(data));
                temp1->numWriteSuccess++;
            }    
            else 
                temp1->numWriteFailure++;
            break;
        case HE_CHILD_CTX_FILE_CLOSE_FLAG :
             if(HE_SYSTEM_CALL_COMP == start)
                temp1->isOpenCloseSuccess |= (opStatus == HE_OP_SUCCESS)?HE_FILE_CLOSE_SUCCESS :HE_FILE_CLOSE_FAILURE ;  
            break ;
        default :
             return -1;
   
    }

    temp1->flag |=flag; 

    if(newNode)
        (pChdCtx->pfileTable)[fd]->last = temp1;

    return 0;
       
}


/*
    API for updating the socket table entry in the child context . 
*/

int he_update_chd_sd_table(int sd , he_child_ctx *pChdCtx, uint32_t flags,void *data)
{
    he_sock_table_entry** temp = NULL;
    he_sock_table_entry* temp1 = NULL;
    char * file = NULL;
    uint8_t newNode =0;

    uint32_t sizeTocopy ;
    if(fd < 0 || sd > HE_MAX_SD_PER_PROCESS || NULL == pChdCtx  )
    {
        return -1 ;
    }

    if(NULL == pChdCtx->psockTable)
    { 
        pChdCtx->psockTable =(he_ChildCtxiSockTableBucket*) malloc (HE_MAX_SD_PER_PROCESS* sizeof(he_ChildCtxSockTableBucket));
        if(NULL  == pChdCtx->psockTable)
        {
           return -1 ;			
        }
        memset(pChdCtx->psockTable, 0,sizeof(HE_MAX_SD_PER_PROCESS* sizeof(he_ChildCtxFileTableBucket)));
      
    }
  
    /*if the file is being opened then only allocate the file table entry from the 
     file table bucket*/
    if(flags == HE_CHILD_CTX_SOCKET_CREATION_FLAG && HE_SYSTEM_CALL_BEGINING == start)
    { 
        temp = &((pChdCtx->psockTable)[sd].first);
        temp1 = (he_file_table_entry*)he_get_Node((he_simple_node **)temp,sizeof(he_file_table_entry));
        newNode =1;
    }
    else 
    {
       /*Take the last node in the list , because it will be the file table entry which is active for that fd*/
       temp1 = (pChdCtx->pfileTable)[fd]->last;
    } 

    if(NULL == temp1)
    {
       printf("(fn %s) Failed to get the node for he_file_table_entry",__func__);
       return -1 ;
    }
   
    switch(flags)
    {
        case HE_CHILD_CTX_SOCKET_CREATION_FLAG :
             file = (char *)data; 
             sizeTocopy = (strlen(file) >HE_MAX_FILE_NAME_SIZE)?HE_MAX_FILE_NAME_SIZE :strlen(file);
             memcpy(temp1->fname, file , sizeTocopy);
             temp1->fname[sizetocopy] = '\0';
             break;
        case HE_CHILD_CTX_SOCKET_CONNECT_FLAG :
             
             break;
        case HE_CHILD_CTX_SOCKET_READ_FLAG :
            (temp1->dataRead)+= *((uint32_t*)(data));
            break;
        case HE_CHILD_CTX_SOCKET_WRITE_FLAG :
            (temp1->dataWritten)+= *((uint32_t*)(data));
            break;
        case HE_CHILD_CTX_SOCKET_CLOSE_FLAG :
            if(HE_SYSTEM_CALL_COMP == start)
               temp1->isConCloseSuccess |= (opStatus == HE_OP_SUCCESS)?HE_FILE_CLOSE_SUCCESS :HE_FILE_CLOSE_FAILURE ;

             break ;
        default :
             return -1;
   
    }

    temp1->flag |=flag; 

    if(newNode)
        (pChdCtx->pfileTable)[fd]->last = temp1;

    return 0;
       
}

/*
    API for updating the file name in the config structure 
*/
/*
    API for updating the file name in the config structure 
*/

int he_UpdateFileToMonInCfg(he_Config * pCfg , char * pfile )
{
   uint32_t index =0;
   he_MonFileEntry *pFileEntry = NULL ;
   int fileSize =0;

   if(NULL == pCfg || NULL == pfile )
   {
       return -1 ; 
   }

   /*Calaulate the hash based on the file name */
   index = he_GetHashIndexForGivenString(pfile, strlen(pfile),HE_CONFIG_FD_TABLE_MAX_SIZE);
   if(index > HE_CONFIG_FD_TABLE_MAX_SIZE )
   {
#ifdef DEBUG_HE_CONFIG 
      printf("\n(fn %s) wrong index for file %s ",__FUNC__, pFile);
#endif 
      return -1 ;

   }

   /*Insert the file name in the hash table */
  
   pFileEntry = he_get_Node(&(pCfg->pMonFileTable[index]), sizeof(he_MonFileEntry));

   if(NULL == pFileEntry)
   { 
#ifdef DEBUG_HE_CONFIG 
      printf("(fn %s) he_get_Node failed to get the node ", __FUNC__ );
#endif   
      return -1 ;
   }
   
   /*update the file name */
  memset(pFileEntry->file,0,sizeof(pFileEntry->file));
  fileSize = strlen(pfile) > HE_MAX_FILE_NAME_SIZE ?HE_MAX_FILE_NAME_SIZE :strlen(pfile);
  memcpy(pFileEntry->file,pfile,fileSize);
#ifdef DEBUG_HE_CONFIG
    printf("(fn %s) copying the file name %s into the config ",__FUNC__,pfile);
#endif  
  return 0;

   
}

/*
    API for updating the BL ip address  in the config structure 
*/

int he_UpdateBLIpInCfg(he_Config * pCfg , char * pip )
{
   uint32_t index =0;
   he_MonFileEntry *pFileEntry = NULL ;
   int fileSize =0;
   uint32_t ip =0;

   if(NULL == pCfg || NULL == pip )
   {
       return -1 ; 
   }

   /*Calaulate the hash based on the file name */
   index = he_GetHashIndexForGivenString(pip, strlen(pip),HE_CONFIG_SD_TABLE_MAX_SIZE);
   if(index > HE_CONFIG_SD_TABLE_MAX_SIZE )
   {
#ifdef DEBUG_HE_CONFIG 
      printf("\n(fn %s) wrong index for file %s ",__FUNC__, pFile);
#endif
      return -1 ;

   }

   /*Insert the ip in the hash table */

   pFileEntry = he_get_Node(&(pCfg->pBLIPEntry[index]), sizeof(he_BLIpEntry));

   if(NULL == pFileEntry)
   {
#ifdef DEBUG_HE_CONFIG 
      printf("(fn %s) he_get_Node failed to get the node ", __FUNC__ );
#endif
      return -1 ;
   }

   /*update the ip */
  ip  = inet_addr(pip);
  pCfg->ip = ip ; 
#ifdef DEBUG_HE_CONFIG
    printf("(fn %s) copying the ip  %s into the config ",__FUNC__,pip);
#endif
  return 0;

}

/*
   API for updating the ops in the config structure
*/

int he_UpdateOpsInCfg(he_Config * pCfg , uint32_t score ,uint32_t opScope , uint32_t op )
{
    he_ops *pOps = NULL ;


    if(NULL == pCfg)
    {
       return -1 ;
    }

   /*Insert the operation in the table*/
   pOps = he_get_Node(&(pCfg->pOps[op]), sizeof(he_ops));

   if(NULL == pOps)
   {
#ifdef DEBUG_HE_CONFIG 
      printf("(fn %s) he_get_Node failed to get the node ", __FUNC__ );
#endif
      return -1 ;
   }

   /*Update the Ops node */
   pOps->score = score;
   pOps->opScope =opScope ;

   return 0;
   
}

/*
   API for registering the parsing of the tags 
*/

int he_RegisterTagAndParseFn(char *startTag ,char *endTag ,uint32_t idx, CfgParsingfn fn , he_tagPraseFnStruct *ptagPraseFnStruct)
{
     if(NULL == startTag || NULL == endtag || NULL == fn || NULL  == ptagPraseFnStruct)
     {
        return -1 ;
     }
    
     if(strlen(startTag)  > HE_CONFIG_TAG_MAX_SIZE || strlen(endTag) > HE_CONFIG_TAG_MAX_SIZE)
     {
        return -1 ; 
     }
     memset(&ptagPraseFnStruct[idx],0,sizeof(he_tagPraseFnStruct));
     memcpy(ptagPraseFnStruct[idx].startTag,startTag,strlen(startTag));     
     memcpy(ptagPraseFnStruct[idx].endTag,endTag,strlen(endTag));
     ptagPraseFnStruct[idx].parseFn = fn;
     
     return 0;
}

#define HE_CONFIG_TAG_START_FOUND    0x01
#define HE_COINFIG_TAG_END_FOUND     0x02

/*API for parsing the cnfiguration file and updating the configuartion structure*/

int he_ParseCfgFile(char *file , he_Config *pCfg)
{
     FILE *fp  =0;
     char *temp = NULL ;
     char buf[HE_CONFIG_TAG_MAX_SIZE+1] ={0,};
     int index =0;    
     int ret =0;

     if(NULL  ==  file || NULL == pCfg)
     {
        return -1;
     }
     /*Register the tag parsing function */
     he_RegisterTagAndParseFn("<ENTITY_SB_ROOT_FOR_CHILD_START>","<ENTITY_SB_ROOT_FOR_CHILD_END>",HE_CONFIG_TAG_ROOT_FOR_CHILD,
    ,he_parseRootForChildTag,gTagParseFnTable);
     he_RegisterTagAndParseFn("<ENTITY_BLACK_LISTED_IP_START>","<ENTITY_BLACK_LISTED_IP_END>",HE_CONFIG_TAG_BLACK_LISTED_IP,
    ,he_parseBLIPTag,gTagParseFnTable);
     he_RegisterTagAndParseFn("<ENTITY_FILE_TO_MONITER_START>","<ENTITY_FILE_TO_MONITER_END>",HE_CONFIG_TAG_FILES_TO_MONITER,
    ,he_parseFileToMonitorTag,gTagParseFnTable);
     he_RegisterTagAndParseFn("<ACTIVITY_TRACK_OPERATIONS_ON_MONITERED_FILE_START>","<ACTIVITY_TRACK_OPERATIONS_ON_MONITERED_FILE_END>",HE_CONFIG_TAG_TRACK_OP_ON_MON_FILE,    ,he_parseTrackOpsOnMOnFilesTag,gTagParseFnTable);
     he_RegisterTagAndParseFn("<ACTIVITY-TRACK_OPERATIONS_ON_BLACK_LISTED_IP_PORT_START>","<ACTIVITY-TRACK_OPERATIONS_ON_BLACK_LISTED_IP_PORT_END>",HE_CONFIG_TAG_TRACK_OP_ON_BL_IP_PORT,he_parseTrackOpsOnMOnBLIpTag,gTagParseFnTable);  
     he_RegisterTagAndParseFn("<ACTIVITY_TRACK_OPERATIONS_START>","<ACTIVITY_TRACK_OPERATIONS_END>",HE_CONFIG_TAG_TRACK_OP,he_parseTrackOpsTag,gTagParseFnTable); 
     he_RegisterTagAndParseFn("<CONFIG_VALUES_THRESHOLD_SCORE_START>","<CONFIG_VALUES_THRESHOLD_SCORE_END>",HE_CONFIG_TAG_THRESHOLD,he_parseThresholdTag,gTagParseFnTable); 

    
     /*Open the configuration file */
     fp= fopen(file,"r" );
     if(NULL == fp)
     {
        printf("\n(fn %s) failed to open the config file %s",__func__,file);
        return -1 ;
     }
    
     /*Start reading the lines and checking if the line contains registered tag*/
     while ((temp = fgets(buf,HE_CONFIG_TAG_MAX_SIZE,fp)!= NULL))
     {
        /*Remove the \n */
        HE_CLEAR_NEW_LINE_FROM_STR(buf);

       /*Check the tag to which the line matches and then call the appropriate tag handler*/
       index = he_getParsing(buf,strlen(buf),gTagParseFnTable); 
       if(!(index >= 0 && index <HE_CONIG_MAX_NUMBER_OF_TAGS))
       {
          printf("\n(fn %s) failed to get the farse function for tag %s",__func__,buf);
          return -1;
       }
       ret=gTagParseFnTable[index].parseFn(fp,&gCfg,buf,gCfg[index].endTag,flag);
       if(0 != ret)
       {
         printf("\n(fn %s) Failed to parse the body of tag %s",__func__,buf);
         return -1 ;
       }
            
     }

     return 0;
}


/*Below we ahve apis which will be used to parse the tags . These apis will be called for individual tag 
 These api will be parsing the file and updating the configuration for a specific tag*/

/*API for parsing the root folder for the child tag*/
int he_parseRootForChildTag(FILE *file ,he_Config *pCfg , char *StartTag ,char *EndTag , uint32_t flag)
{
   char line[HE_CONFIG_TAG_DATA_LINE_MAX_SIZE+1] ={0,};
   char *temp = NULL ;
   char temp2 = NULL;
   uint8_t endReached = 0;
   uint8_t readRoot = 0;

   if(NULL  ==  pCfg || NULL ==  StartTag || NULL == EndTag )
   {
#ifdef  DEBUG_HE_CONFIG   
        printf("\n (fn %s) Invalid input to the function",__func__);
#endif 
        return -1;
   }

   while(endReached != 1)
   {
        /*read one line from the configuration  file */
        temp = fgets(line,HE_CONFIG_TAG_DATA_LINE_MAX_SIZE+1,file);
        if(NULL == temp)
        {
#ifdef  DEBUG_HE_CONFIG   
            printf("\n (fn %s) Invalid configuration for tag %s",__func__,StartTag);
#endif       
            return -1 ;
       }

       /*Remove the \n if required*/
       HE_CLEAR_NEW_LINE_FROM_STR(line) ;
       /*Check if the its end tag*/
       temp2 = strcasestr(line,EndTag) ;
       if(NULL != temp2 && line == temp2 && (strlen(EndTag) == strlen(line)))
       {
          endReached = 1;
       }
       else
       {
            /*Update configuratin structure*/
#ifdef DEBUG_HE_CONFIG
            printf("(fn %s) update config with root folder %s",__func__,line)
#endif 
            memcpy(pCfg->rootForChild,line,strlen(line));
       }

   }

   return 0;

}

/*Api for parsing the Blocked ip list*/
int he_parseBLIPTag(FILE *file ,he_Config *pCfg , char *StartTag ,char *EndTag , uint32_t flag)
{
   char line[HE_CONFIG_TAG_DATA_LINE_MAX_SIZE+1] ={0,};
   char *temp = NULL ;
   char temp2 = NULL;
   uint8_t endReached = 0;
   uint8_t readRoot = 0;
   struct in_addr ip= {0,}; 
   int ret =0;
   he_BLIpEntry *pIpEntry = NULL ;
   uint32_t index =0;

   if(NULL  ==  pCfg || NULL ==  StartTag || NULL == EndTag )
   {
#ifdef  DEBUG_HE_CONFIG   
        printf("\n (fn %s) Invalid input to the function",__func__);
#endif
        return -1;
   }

   while(endReached != 1)
   {
        /*read one line from the configuration  file */
        temp = fgets(line,HE_CONFIG_TAG_DATA_LINE_MAX_SIZE+1,file);
        if(NULL == temp)
        {
#ifdef  DEBUG_HE_CONFIG   
            printf("\n (fn %s) Invalid configuration for tag %s",__func__,StartTag);
#endif
            return -1 ;
       }

       /*Remove the \n if required*/
       HE_CLEAR_NEW_LINE_FROM_STR(line) ;
       /*Check if the its end tag*/
       temp2 = strcasestr(line,EndTag) ;
       if(NULL != temp2 && line == temp2 && (strlen(EndTag) == strlen(line)))
       {
          endReached = 1;
       }
       else
       {
            /*Update configuratin structure*/

            ret = inet_aton(line,&ip);
            if(0 ==ret)
            {
#ifdef DEBUG_HE_CONFIG
                 printf("(fn %s) error occured while converting the ip %s",
               __func__,line);
#endif
                return -1 ;
            }
  
#ifdef DEBUG_HE_CONFIG
            printf("(fn %s) now will be updating the ip address %08x in the configuration ",
               __func__,ip)
#endif
            index = he_GetHashIndexGoGivenIP(ip.s_addr,sizeof(ip.s_addr),HE_CONFIG_BL_IP_HASH_TABLE_MAX_SIZE);  
            pIpEntry = he_get_Node(&(pCfg->pBLIPEntry[index]),sizeof(he_BLIpEntry));
            pIpEntry->ip= ip.s_addr ;
            
       }

   }

   return 0;

}

/*API for parsing the files the files to moniter from the configuration file */
int he_parseFileToMonitorTag(FILE *file ,he_Config *pCfg , char *StartTag ,char *EndTag , uint32_t flag)
{
   char line[HE_CONFIG_TAG_DATA_LINE_MAX_SIZE+1] ={0,};
   char *temp = NULL ;
   char temp2 = NULL;
   uint8_t endReached = 0;
   uint8_t readRoot = 0;
   uint32_t index =0;
   he_MonFileEntry *pFileEntry = NULL ;


   if(NULL  ==  pCfg || NULL ==  StartTag || NULL == EndTag )
   {
#ifdef  DEBUG_HE_CONFIG   
        printf("\n (fn %s) Invalid input to the function",__func__);
#endif
        return -1;
   }

   while(endReached != 1)
   {
        /*read one line from the configuration  file */
        temp = fgets(line,HE_CONFIG_TAG_DATA_LINE_MAX_SIZE+1,file);
        if(NULL == temp)
        {
#ifdef  DEBUG_HE_CONFIG   
            printf("\n (fn %s) Invalid configuration for tag %s",__func__,StartTag);
#endif
            return -1 ;
       }

       /*Remove the \n if required*/
       HE_CLEAR_NEW_LINE_FROM_STR(line) ;
       /*Check if the its end tag*/
       temp2 = strcasestr(line,EndTag) ;
       if(NULL != temp2 && line == temp2 && (strlen(EndTag) == strlen(line)))
       {
          endReached = 1;
       }
       else
       {
          /*Calculate the hash table index based on the file name */
          index = he_GetHashIndexForGivenString(line ,strlen(line)i,HE_CONFIG_FILE_HASH_TABLE_MAX_SIZE);  
          pFileEntry = he_get_Node(&(pCfg->pMonFileTable[index]),sizeof(he_MonFileEntry));
          if(NULL ==  pFileEntry)
          {
             printf("\n(fn %s) failed to get the Node for storing file to monitor ",__func__) ;
             return - 1;
          }
          memcpy(pFileEntry->file ,line,HE_MAX_FILE_NAME_SIZE);
          printf("\n(fn %s) updated file %s in He Config at index %d", pFileEntry->file,index);
          
       }

   }

   return 0;
}

/*API for parsing the threshold*/

int he_parseThresholdTag(FILE *file ,he_Config *pCfg , char *StartTag ,char *EndTag , uint32_t flag)
{
   char line[HE_CONFIG_TAG_DATA_LINE_MAX_SIZE+1] ={0,};
   char *temp = NULL ;
   char temp2 = NULL;
   uint8_t endReached = 0;
   uint8_t readRoot = 0;

   if(NULL  ==  pCfg || NULL ==  StartTag || NULL == EndTag )
   {
        printf("\n (fn %s) Invalid input to the function",__func__);
        return -1;
   }

   while(endReached != 1)
   {
        /*read one line from the configuration  file */
        temp = fgets(line,HE_CONFIG_TAG_DATA_LINE_MAX_SIZE+1,file);
        if(NULL == temp)
        {
            printf("\n (fn %s) Invalid configuration for tag %s",__func__,StartTag);
            return -1 ;
       }

       /*Remove the \n if required*/
       HE_CLEAR_NEW_LINE_FROM_STR(line) ;
       /*Check if the its end tag*/
       temp2 = strcasestr(line,EndTag) ;
       if(NULL != temp2 && line == temp2 && (strlen(EndTag) == strlen(line)))
       {
          endReached = 1;
       }
       else
       {
            /*Update configuratin structure*/
            pCfg->thrScore = atoi(line);
            printf("(fn %s) update config with threshold  %d",__func__,pCfg->thrScore);
       }

   }

   return 0;

}

/*APi which will be used for parsing the track operations on monitered files tag*/
int he_parseTrackOpsOnMOnFilesTag(FILE *file ,he_Config *pCfg , char *StartTag ,char *EndTag , uint32_t flag)
{

   char *temp = NULL ;
   char *temp2 =  NULL ;
   uint32_t score =0;
    uint32_t op=0;
   uint32_t subOp =0;

   if(NULL  ==  pCfg || NULL ==  StartTag || NULL == EndTag )
   {
        printf("\n (fn %s) Invalid input to the function",__func__);
        return -1;
   }

   while(endReached != 1)
   {
        /*read one line from the configuration  file */
        temp = fgets(line,HE_CONFIG_TAG_DATA_LINE_MAX_SIZE+1,file);
        if(NULL == temp)
        {
            printf("\n (fn %s) Invalid configuration for tag %s",__func__,StartTag);
            return -1 ;
       }

       /*Remove the \n if required*/
       HE_CLEAR_NEW_LINE_FROM_STR(line) ;
       /*Check if the its end tag*/
       temp2 = strcasestr(line,EndTag) ;
       if(NULL != temp2 && line == temp2 && (strlen(EndTag) == strlen(line)))
       {
          endReached = 1;
       }
       else
       {
            /*Update configuration structure*/
            if(0 != he_parseUpdateOpLineInternal(pCfg,line,strlen(line),HE_CONFIG_OP_ON_MON_FILE))
            {
                return -1 ;
            }
       }

   }

   return 0;

}

/* API to parse the track ops on blacklisted ip address*/
int he_parseTrackOpsOnMOnBLIpTag(FILE *file ,he_Config *pCfg , char *StartTag ,char *EndTag , uint32_t flag)
{
   char *temp = NULL ;
   char *temp2 =  NULL ;
   uint32_t score =0;
    uint32_t op=0;
   uint32_t subOp =0;

   if(NULL  ==  pCfg || NULL ==  StartTag || NULL == EndTag )
   {
        printf("\n (fn %s) Invalid input to the function",__func__);
        return -1;
   }

   while(endReached != 1)
   {
        /*read one line from the configuration  file */
        temp = fgets(line,HE_CONFIG_TAG_DATA_LINE_MAX_SIZE+1,file);
        if(NULL == temp)
        {
            printf("\n (fn %s) Invalid configuration for tag %s",__func__,StartTag);
            return -1 ;
       }

       /*Remove the \n if required*/
       HE_CLEAR_NEW_LINE_FROM_STR(line) ;
       /*Check if the its end tag*/
       temp2 = strcasestr(line,EndTag) ;
       if(NULL != temp2 && line == temp2 && (strlen(EndTag) == strlen(line)))
       {
          endReached = 1;
       }
       else
       {
            /*Update configuration structure*/
            if(0 != he_parseUpdateOpLineInternal(pCfg,line,strlen(line),HE_CONFIG_OP_ON_BL_IP))
            {
                return -1 ;
            }
       }

   }

   return 0;
}

/*API to parse the trask ops tag*/
int he_parseTrackOpsTag(FILE *file ,he_Config *pCfg , char *StartTag ,char *EndTag , uint32_t flag)
{
   char *temp = NULL ;
   char *temp2 =  NULL ;
   uint32_t score =0;
    uint32_t op=0;
   uint32_t subOp =0;

   if(NULL  ==  pCfg || NULL ==  StartTag || NULL == EndTag )
   {
        printf("\n (fn %s) Invalid input to the function",__func__);
        return -1;
   }

   while(endReached != 1)
   {
        /*read one line from the configuration  file */
        temp = fgets(line,HE_CONFIG_TAG_DATA_LINE_MAX_SIZE+1,file);
        if(NULL == temp)
        {
            printf("\n (fn %s) Invalid configuration for tag %s",__func__,StartTag);
            return -1 ;
       }

       /*Remove the \n if required*/
       HE_CLEAR_NEW_LINE_FROM_STR(line) ;
       /*Check if the its end tag*/
       temp2 = strcasestr(line,EndTag) ;
       if(NULL != temp2 && line == temp2 && (strlen(EndTag) == strlen(line)))
       {
          endReached = 1;
       }
       else
       {
            /*Update configuration structure*/
            if(0 != he_parseUpdateOpLineInternal(pCfg,line,strlen(line),HE_CONFIG_OP_NORMAL))
            {
                return -1 ;
            }
       }

   }

   return 0;
}

int he_updateOpsTable(he_Config *pCfg ,uint32_t ops, uint8_t ifSubOpsValid ,uint32_t subOps, uint32_t score, uint32_t opScope)
{
      he_ops *pOpsNode = NULL ;
 
      if(NULL == pCfg)
      {
          return -1;
      } 

     if(ops >=HE_CONFIG_MAX_OPS)
     {
        printf("\n(fn %s) Invalid operation %d",__func__,ops);
        return -1;
     }

     pOpsNode =(he_ops*)he_get_Node(&(pCfg->pOps[ops]),sizeof(he_ops));
     if(NULL == pOpsNode)
     {
         printf("\n(fn %s) Failed to get Node for ops",__func__);
         return -1;
     }
     pOpsNode->opScope = opScope;
     pOpsNode->score = score;
     if(ifSubOpsValid)
     {
        pOpsNode->subOp = subOps;
        pOpsNode->flags |=HE_ENG_OP_FLAG_SUB_OP;
     }

     return 0;
    
}


int he_StrIsNum(char* str , int len)
{
    int isNum =1;
    int i =0;
    
    while(len>0)
    {
       if(!isdigit(str[i]))
       {
          return 0;
       }
       len--;
       i++;
    }
    return 1;
}

int he_GetIdForOp(char *op ,int len,uint32_t *outOp , uint32_t* subOp)
{
    int i =0;

    if(NULL == op|| NULL == outop)
    {
        return -1;
    }

   for(i=0;i<HE_CONFIG_MAX_OP_TO_ID_MAPPING;i++)
   {
      if(!strcmp(op,gOpToNumMap[i].opStr))
      {
           *outOp = gOpToNumMap[i].op;
           *subOp=gOpToNumMap[i].subOp;
            return 0;
      }
   }
   return -1;

}

/*
    This API will be parsing the operation line and updating the configuration .
    the opration line will be of the format <operation>:<score>
*/
int he_parseUpdateOpLineInternal(he_Config *pCfg,char *opLine, uint32_t len , uint32_t scope)
{
   char *temp = NULL ;
   char *temp2 =  NULL ;
   uint32_t score =0;
   uint32_t op=0;
   uint32_t subOp =0;
   int ret =0;

   if(NULL opLine || len ==0)  
   {
       return -1 ;
   }

   /*Update configuration structure*/
   temp =strchr(line,HE_CONFIG_DELIMITER);
   if(NULL == temp)
   {
        /*Score is not mentioned for the operation*/
        score =0;
   }
   else
   {
        *temp = '\0';
         temp++;
        /*Check for the Validity of the score */
         if(!he_StrIsNum(temp,strlen(temp)))
         {
             printf("\n(fn %s) The score is not a number (%s)",__func__,temp);
             return -1;
         }
         score = atoi(temp);
   }
   /*get the number for the operation*/
   if(0 != he_GetIdForOp(line,strlen(line),&op,&subOp))
   {
         printf("\n (fn %s) failed to get the if for op %s",__func__,line);
         return -1 ;
   }
   
   ret=he_updateOpsTable(pCfg,op,subop !=0 ?1:0,sunOp,score,scope);
   if(0 !=ret)
   {
      printf("\n (fn %s) Failed to update the operation in the configuration table ",__func__);
      return -1;
   }
   
   return 0;

}


/* API will returning the index of the tag in the tag<->parse fuction mapping*/

int  he_getParsing(char *tagLine , int taglen, he_tagParseFnStruct *pTagParseFnTable )
{
    int i =0;
    if(NULL == tagLine || taglen ==0 || NULL == pTagParseFnTable)
    {
       return -1 ;
    }
 
    for(i=0;i<HE_CONIG_MAX_NUMBER_OF_TAGS;i++)
    {
        if(!strcmp(tagline,pTagParseFnTable[i].startTag))
        {
            return i;
        }
    }

    return -1;
           
}

#define HE_USER  "he_user"
#define HE_GROUP "he_group"

/*API which will be putting the calling process in a sandbox .Befomre calling this api we ahve to make sure that
a normal user "he_user" and group "he_group" shoudl be created*/
int he_setSandBox(he_Config *pCfg)
{
   int ret =0;
   struct passwd *pwd = NULL ; 

   if(NULL == pCfg)
      return -1 ;

   /*change the root directory and cwd of the process */
    ret = chroot(pCfg->rootForChild);
    if(0 != ret)
    {
       printf("\n(fn %s) failed to change the root to %s",__func__,pCfg->rootForChild);
       return -1 ;
    }
      
   /*Change the euid and egid of the process*/
   pwd = getpwnam(HE_USER);
   if(NULL == pwd)
   {
       printf("\n(fn %s) Failed to get passwd entry for user %s",__func__,HE_USER);
       return -1 ;
   }
   ret = seteuid(pw->pw_uid);
   if(0 != ret)
   {
      printf("\n(fn %s) Failed to setieuid for uid %d and user %s",__func__,pw->pw_uid,HE_USER);
      return -1 ;
   }
   ret = setegid(pw->pw_gid);
   if(0 != ret)
   {
      printf("\n(fn %s) Failed to setegid for gid %d and user %s",__func__,pw->pw_gid,HE_USER);   
      return -1 ;
   }
 
   return 0; 
}

/*
   Api to check if the descriptor is a file or socket descriptor
*/

int he_CheckIfFileFdOrSd(struct user_regs_struct *pRegs , he_child_ctx *pChdCtx , int* fdType )
{
    he_ChildCtxFileTableBucket *pBucket = NULL ;
    he_ChildCtxiSockTableBucket *pSockBucket = NULL ;
    he_file_table_entry *pEntry = NULL ;
    he_sock_table_entry *pSEntry = NULL ;
    unsigned long *pArgs = NULL ;

    if(NULl == pChdCtx || NULL == fdType || NULL == pRegs)
    {
       printf("(fn %s) Invalid input",__func__);
       return -1;
    }


   *fdType = HE_FD_TYPE_IS_NONE ;
   if(!IS_SYS_CALL_SOCKET_TYPE(pregs))
   {   
      /*Check in the file discriptor table */
      if(NULL !=  pChdCtx->pfileTable && ((pregs->ebx) >= 0 && (pregs->ebx)< HE_MAX_FD_PER_PROCESS))
      {
         pBucket = &((pChdCtx->pfileTable)[pregs->ebx]);
         if(NULL != pBucket)
         {
            pEntry = pBucket->last ;
            if(0 !=(HE_CHILD_CTX_FILE_CLOSE_FLAG & pEntry->flag))
            {
              *fdType = HE_FD_TYPE_IS_FILE ;
               return 0;
            }
        }
     }
   }
   else 
   {
       /*we are assuming that for all the functions implemented by the socket call will be having the 
         first part in the second argument as file descriptor */

      /*Chekc if the operation performed by the socketcall is valid or not */
      if(IS_SOCKET_CALL_VALID_OP(pregs))
      {
         /*AKSHAR_TBD extract the file descriptor */
          

         /*Check in the socket descriptor table */
         if(NULL !=  pChdCtx->psockTable && (fd > 0 && fd < HE_MAX_SD_PER_PROCESS))
         {
            pSockBucket = &((pChdCtx->psockTable)[fd]);
            if(NULL != pSockBucket)
            {
               pSEntry = pBucket->last ;
               if(0 !=(HE_CHILD_CTX_SOCKET_CLOSE_FLAG & pSEntry->flag))
               {
                  *fdType = HE_FD_TYPE_IS_SOCKET ;
                   return 0;
               }
           }
        }
      }

   }
   return 0;
}

/*
    API which will be converting sys call num and suboperation to local vaules 
*/

int he_getOpLocalNum(int sys , int subOP , int *locOpNum,int fdType)
{
    if(NULL  ==  locOPNum)
    {
       printf("\n(fn %s) INvaild input\n",__func__);
       return -1;
    }

    if(__NR_socketcall == sys)
    {
        /*Search for the suboperation */
        switch(subOP)
        {
            case SYS_SEND :
            case SYS_SENDTO :
            case SYS_SENDMSG:
                 *locOpNum = HE_CHILD_CTX_SOCKET_WRITE_FLAG ;
                  break;
            case SYS_RECV :
            case SYS_RECVFROM :
            case SYS_RECVMSG :
                 *locOpNum = HE_CHILD_CTX_SOCKET_READ_FLAG ;
                  break;
            default :
                  printf("\n(fn %s) invaild sup op %d for __NR_socketcall",
                     __func__,subOP);
                  return -1;

        } 
    }
    else
    {
         switch (sys)  
         {                   
            case __NR_open :                                 
               *locOpNum = HE_CHILD_CTX_FILE_OPEN_FLAG ;    
                break;                                       
            case __NR_read :                                 
               if(HE_FD_TYPE_IS_FILE == fdType)
                   *locOpNum = HE_CHILD_CTX_FILE_READ_FLAG ;
               else
                    *locOpNum = HE_CHILD_CTX_SOCKET_READ_FLAG ;
               break;                                        
            case __NR_write :                                
              if(HE_FD_TYPE_IS_FILE == fdType)  
                   *locOpNum = HE_CHILD_CTX_FILE_WRITE_FLAG ;         
              else
                   *locOpNum = HE_CHILD_CTX_SOCKET_WRITE_FLAG ;
               break;                                      
         default :                                         
               printf("(fn %s) Invalid sys cal number for error",__func__);
                       return -1;   
      }
    }

   return 0;
}










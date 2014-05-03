/*HEURISTIC ENGINE*/
#include <stdio.h>
#include <unistd.h>    
#include <sys/types.h>
#include <unistd.h>

he_Config gCfg = {0,};

int he_RunExe(char *exe,he_Config *pCfg)
{

   int ret =0;
   if(NULL == exe || NULL == pCfg)
   {
      printf("(fn %s) Invalid input ",__func__);
      return -1;
   }
   /*Set the sandbox parameters */
    ret = he_setSandBox(pCfg) ;
    if(0 != ret)
    {
        printf("(fn %s) Failed to set the sandBox parameters",__func__);
        return -1;
    } 

   /*Cal the ptrace api to trace the exe*/
    ptrace (PTRACE_TRACEME, 0, NULL, NULL);;

   /*exec the exe*/
    execl (exe, "", NULL);

    return 0;
  
}

#define HE_SYSTEM_CALL_BEGINING   0x01
#define HE_SYSTEM_CALL_COMP       0x02

int main(int argc , char *argv)
{
    int ret =0;
    int status =0;
    struct user_regs_struct regs = {0,};
    int child =0;
    int syscall = HE_SYSTEM_CALL_BEGINING;

    /*Parse the configuration file and update the configuration structure */    
    if(3 != argc)
    {
         printf("\n(fn %s)  invalid number of arguments  ",__func__);
         exit(-1);
    }
   
    ret = he_ParseCfgFile(argv[1],&gCfg);
    if(0 != ret)
    {
        printf("\n(fn %s) Failed to parse the config file %s",__func__,argv[1]);
        exit(-1);
    }

    /*Fork and create the chid process which will be running the exe*/

    if((child =fork()) == 0)
    {
         /*Child process so run the executable*/
         he_RunExe(argv[2],&gCfg);
    }
    
    /*Parent Process so wait for status from child */
    while(1)
    {
         wait(&status);
         if(WIFEXITED(status)||WIFSIGNALED(status))
         {
             printf("\n(fn %s) The child process(pid = %d) has exited",__func__, child);
             break;
         }
        
         /*Read the contents of the registers */
         ret = ptrace(PTRACE_GETREGS, child,NULL, &regs);
         if(0 != ret)
         {
              printf("(fn %s) ptrace failed to get reg values %d",errno);
              return -1; 
         } 
         /*Before the system call is called */  
         if(HE_SYSTEM_CALL_BEGINING == syscall)    
         {
             /*Update the relevant data structures before the system call is actually executed*/
             syscall = HE_SYSTEM_CALL_COMP;
         }
         else
         { 
            /*After the system call is called*/
            /*Update the relevant data structure after  the system call is executed*/
            syscall = HE_SYSTEM_CALL_BEGINING;
         }
  
    }
    return 0;
   
}


/*Api which will be handling the processing of the system call both before and after */

int he_sysHandler(he_child_ctx *pChildCtx , he_Config *pCfg ,struct user_regs_struct *pRegs, uint8_t startFlag)
{
    static he_child_op_list *pNode = NULL ;
    int tempLoclOp =0;
    int ret =0;
    int fileType =0;
    uint8_t opStatus =0;

    if(NULL == pChildCtx || NULL == pCfg || NULL == pRegs)
      return -1 ;

     if(HE_SYSTEM_CALL_BEGINING == startFlag)
     {
         /*Create the a new node in the operation list */
         pNode = (he_child_op_list*)he_get_Node((he_simple_node **)&(pChildCtx->pChildops),sizeof(he_child_op_list));
         pNode->op = pRegs->orig_eax ;
         /*Check there are sub operations , like in case of system calls like socketcall and ipc*/
         if(HE_DOES_OP_HAVE_SUB_OP(pNode->op))
         { 
             pNOde->subOp = pRegs->ebx;
         }
         return OK ;
  
     }
     else
     {
         /*Update the operation node with the result */
         pNode->result = pregs->eax;
         opStatus = HE_OP_HAS_BEEN_SUCCESSFUL(pregs->eax);
         if(IS_SYS_CALL_FILE_OPEN(pRegs->orig_eax))
         {
              HE_FILE_OP_SYS_CALLNUM_CONVERT((pRegs->orig_eax),&tempLoclOp); 
             /*In case of open system call the file Descriptor in the child ctx has to be update*/
              ret = he_update_chd_fd_table(pregs->ebx,pChildCtx,tempLoclOp,NULL);   
         }
         /*in case of file read and write operation then the count has to be increased */ 
         else if(IS_SYS_CALL_READ_OR_WRITE_OP(pRegs->orig_eax,pregs->ebx))
         {
             /*Check if the operation was performed on socket or file */
              ret = he_CheckIfFileFdOrSd(pRegs,pChildCtx,&fileType);
              if(-1 == ret )
              {
                  printf("\n(fn %s) failed to get the entry for descriptor %d",__func__,pRegs->ebx);
                  return -1;
              }  
              if(HE_FD_TYPE_IS_NONE == fileType) 
              {
                  printf("\n(fn %s) Failed to get the descriptor type for descriptor %d",pRegs->ebx);
                  return -1;
              }
             
              /*Convert the syscall numbar and subop number from global to local*/
              ret = he_getOpLocalNum(pRegs->orig_eax,pRegs->ebx,&tempLoclOp,fileType);
              if(0 ! = ret) 
              {
                  printf("\n(fn %s) Failed to get the local op number orig_eax = %d ebx %d",
                       __func__, pRegs->orig_eax,pRegs->ebx);
                  return -1;
              } 

              /*Update the descriptor table*/ 
              /*for file descriptor*/
              if(HE_FD_TYPE_IS_FILE == fileType)
              {
                  ret = he_update_chd_fd_table(pregs->ebx,pChildCtx,tempLoclOp,&(pRegs->eax));
                  if( 0 != ret)
                  {
                      printf("(fn %s)  failed to update the child ctx for sys call %d",
                               __func__,pRegs->orig_eax);
                      return -1;
                  }
              }
              else if if(HE_FD_TYPE_IS_SOCKET == fileType)
              {
                  /*for socket descriptor */
                  ret = he_update_chd_sd_table(pregs->ebx,pChildCtx,tempLoclOp,&(pRegs->eax));
                  if( 0 != ret)
                  {
                      printf("(fn %s)  failed to update the child ctx for sys call %d",
                               __func__,pRegs->orig_eax);
                      return -1;
                  }
              }
              else 
              {
                  printf("\n(fn %s) invaild file type %d",__func__,fileType);
                  return -1;
              } 
            
              }   
             /*In case of connect system call the socket descriptor table in the child context has to be updated*/
              else if(IS_SOCKET_CALL_TYPE_CONNECT(pregs))
              {
                                   
              }
     }

    
  
}

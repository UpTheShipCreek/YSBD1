#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {         \
    BF_PrintError(code);    \
    return HP_ERROR;        \
  }                         \
}

// Records size is 49 bytes ~50, we will be able to store around 10 Records per block

int HP_CreateFile(char *fileName){
  int fileDesc;
  void* data;
  HP_info* hpInfo;
  

  CALL_BF(BF_Init(LRU)); //LRU cause why not
  CALL_BF(BF_CreateFile(fileName)); //create the heapfile
  CALL_BF(BF_OpenFile(fileName, &fileDesc)); //lets get its file descriptor so we can use it

  BF_Block* block;                  
  BF_Block_Init(&block); //create block
  CALL_BF(BF_AllocateBlock(fileDesc,block)); //bind it to our heapfile

  data = BF_Block_GetData(block); //get the block pointer
  if(data == NULL){ 
    perror("Block_getData: Failed to fetch data!");
    return -1;
  }

  
  //hpInfo->file_type = 'P'; //file type heaP, only 1 byte for this type information
  hpInfo->file_desc = fileDesc; //save the fle descriptor
  hpInfo->last_block_id = 0; //initialize the last block position
  hpInfo->num_o_blocks = 1; //initialize the block counter

  memcpy(data, hpInfo,sizeof(HP_info)); //write our metadata on the first(last) block
 
  BF_Block_SetDirty(block); //save the data
  CALL_BF(BF_UnpinBlock(block)); 
  //BF_Block_Destroy(&block); 

  CALL_BF(BF_CloseFile(fileDesc)); //close file

  return 0;
}

HP_info* HP_OpenFile(char *fileName){
  int fileDesc;
  void* data;

  BF_Block* block;  
  BF_Block_Init(&block); 

  HP_info* hpInfo;

  CALL_BF(BF_OpenFile(fileName, &fileDesc)); //get file descriptor
  CALL_BF(BF_GetBlock(fileDesc,0,block)); //get the first block

  data = BF_Block_GetData(block);
  if(data==NULL){
    perror("Block_getData: Failed to fetch data!");
    return NULL;
  }

  memcpy(hpInfo,data,sizeof(HP_info)); //get the metadata

  CALL_BF(BF_UnpinBlock(block)); 
  //BF_Block_Destroy(&block); 

  CALL_BF(BF_CloseFile(fileDesc)); //close file
  return hpInfo;
}


int HP_CloseFile( HP_info* hp_info ){
    
  err = BF_CloseFile(hp_info->file_desc);
  if(err != BF_OK){
    BF_PrintError(err);
    return -1;
  }

  free(hp_info);
  return 0;
}

int HP_InsertEntry(HP_info* hp_info, Record record){
  BF_Block* block;  
  BF_Block_Init(&block); 

  BF_GetBlock(hp_info->file_desc, hp_info->last_block_id,block);

  
  return 0;
}

int HP_GetAllEntries(HP_info* hp_info, int value){
   return 0;
}


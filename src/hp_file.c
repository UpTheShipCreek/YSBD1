#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

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

//#define MAX_RECORDS floor((BF_BLOCK_SIZE - sizeof(HP_block_info))/sizeof(Record))
//#define  block_info_margin MAX_RECORDS*sizeof(Record)

int HP_CreateFile(char *fileName){

  CALL_BF(BF_CreateFile(fileName));

  int file_desc;
  CALL_BF(BF_OpenFile(fileName, &file_desc));

  BF_Block* block;
  BF_Block_Init(&block);

  CALL_BF(BF_AllocateBlock(file_desc, block));

  void* data = BF_Block_GetData(block);
  HP_info* hp_info = data;

  hp_info->file_desc = file_desc;
  hp_info->last_block_index = 0; //we only have one block
  //max records, are the records that fit in the block space, minus the reserved space for the metadata divided by the size of the Record structure
  hp_info->max_records = floor((BF_BLOCK_SIZE - sizeof(HP_block_info))/sizeof(Record)); 

  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  return 0;
}

HP_info* HP_OpenFile(char *fileName){

  int file_desc;
  CALL_BF(BF_OpenFile(fileName, &file_desc));

  BF_Block* block;
  BF_Block_Init(&block);

  CALL_BF(BF_GetBlock(file_desc,0,block)); //getting the first block

  HP_info* hp_info = malloc(sizeof(HP_info));
  
  void* data = BF_Block_GetData(block);
  if(data == NULL){
    //printf("Error: The block is empty!\n");
    return NULL;
  }
  memcpy(hp_info,data,sizeof(HP_info)); //copy the info from the block to the memory allocated for hp_info
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);
  return hp_info; //need to free that someday
}


int HP_CloseFile( HP_info* hp_info ){
  CALL_BF(BF_CloseFile(hp_info->file_desc));
  free(hp_info); //it will get freed when the file closes, makes sense actually
  return 0;
}

int HP_InsertEntry(HP_info* hp_info, Record record){

  //printf("%d\n", __LINE__);

  BF_Block* block0;
  BF_Block_Init(&block0);
  int block_info_margin = hp_info->max_records*sizeof(Record); //okay i'll need to define this at the start of every function it seems


  //printf("%d\n", __LINE__);
  
  //loading Block 0
  CALL_BF(BF_GetBlock(hp_info->file_desc,0,block0));
  //finding the last block in the hp
  void* d = BF_Block_GetData(block0);
  HP_info* info = d; //straight to the source baby 

  //printf("%d\n", __LINE__);
  
  BF_Block* block;
  BF_Block_Init(&block);

  //printf("%d\n", __LINE__);

  if(hp_info->last_block_index == 0){ //if the index of the last block is zero it means that there is only one block and we need to allocate another one
    CALL_BF(BF_AllocateBlock(hp_info->file_desc, block));

    //printf("%d Allocating new block for entry\n", __LINE__);

    //assign the new block the initial metadata
    void* temp = BF_Block_GetData(block);
    HP_block_info* bi = temp + block_info_margin;

    //printf("%d\n", __LINE__);
    
    bi->num_o_records = 0;
    BF_Block_SetDirty(block);

    //printf("%d\n", __LINE__);

    //update the hp's metadata
    info->last_block_index++;
    //printf("%d Last block's index is %d\n", __LINE__,info->last_block_index);
    BF_Block_SetDirty(block0);
    void* nd = BF_Block_GetData(block0); //I hope this updates the hp metadata
    info = nd; //I do that in order to not mess with memcpy I hope it works 
  }
  else{
    CALL_BF(BF_GetBlock(hp_info->file_desc,info->last_block_index,block));
  }
  //printf("%d\n", __LINE__);

  //continue with the newly allocated or the already existing block
  void* data = BF_Block_GetData(block);
  HP_block_info* block_info = data + block_info_margin;

  //printf("%d\n", __LINE__);

  if(block_info->num_o_records < hp_info->max_records){ //if the block can take new records
    //printf("%d The block can take new records\n", __LINE__);
    int margin = block_info->num_o_records*sizeof(Record); 
    memcpy(data+margin, &record, sizeof(Record)); //add the new record in the correct position (I hope)
    //Record* r = data+margin;
    //printf("%d Added record with Id %d\n", __LINE__, r->id);
    block_info->num_o_records++; //increase the record counter
    //printf("%d New number of records is %d\n", __LINE__, block_info->num_o_records);
    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
  }
  else{

    //printf("%d\n", __LINE__);
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);

    //printf("%d\n", __LINE__);

    BF_Block* new_block;
    BF_Block_Init(&new_block);
    CALL_BF(BF_AllocateBlock(hp_info->file_desc, new_block)); //allocate new block

    //printf("%d\n", __LINE__);

    void* new_data = BF_Block_GetData(new_block);
    HP_block_info* bi = new_data +  block_info_margin; //assign the new block the initial metadata

    //printf("%d\n", __LINE__);

    memcpy(new_data, &record, sizeof(Record)); //copy the record at the block's start

    //printf("%d\n", __LINE__);

    bi->num_o_records = 1; //update the metadata
    BF_Block_SetDirty(new_block);

    //printf("%d\n", __LINE__);

    CALL_BF(BF_UnpinBlock(new_block));

    //printf("%d\n", __LINE__);

    BF_Block_Destroy(&new_block);

    info->last_block_index++; //update the hp's metadata
  }
  memcpy(hp_info,info,sizeof(HP_info)); //update the external hp_info structure with the resolution of the operation
  //printf("%d\n", __LINE__);

  BF_Block_SetDirty(block0);
  CALL_BF(BF_UnpinBlock(block0));
  BF_Block_Destroy(&block0);

  //printf("%d\n", __LINE__);

  return 0;
}

int HP_GetAllEntries(HP_info* hp_info, int value){
  bool found = false;
  int block_info_margin = hp_info->max_records*sizeof(Record); //here we go again, all this decause #define had some problems that I couldn't bother with
  int i, j,block_number;
  

  //printf("\n%d Last Block index: %d\n",__LINE__, hp_info->last_block_index);

  for(i = 1; i <= hp_info->last_block_index; i++){ //for every block that is not the first one 
    //printf("%d\n", __LINE__);

    BF_Block* block;
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(hp_info->file_desc,i,block));

    //printf("%d\n", __LINE__);
    void* data = BF_Block_GetData(block);
    HP_block_info* block_info = data + block_info_margin; //getting the block's metadata the good ol' way

    //printf("%d Number of records: %d\n",__LINE__, block_info->num_o_records);
  
    for(j = 0; j < block_info->num_o_records; j++){ //and for every record in this block
      //printf("%d\n", __LINE__);
      Record* record = data + j*sizeof(Record); //get the record
      if(record->id == value){ //and check if it has the Id we want
        //printf("%d\n", __LINE__);
        found = true; // we are setting over and over but it's okay, it is just a boolean
        block_number = i; //the block number we need to update of course
        //printf("Id: %d\n Name: %s\n Surname: %s\n City: %s\n", record->id,record->name,record->surname,record->city);
      }
    }
    //printf("%d\n", __LINE__);
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
  }
  
  if(found){
    //printf("%d\n", __LINE__);
    return block_number;
  }
  else return -1;
}



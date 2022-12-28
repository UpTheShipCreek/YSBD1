#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bf.h"
#include "ht_table.h"
#include "record.h"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }


int hash(int id, HT_info* header_info){
    return id % header_info->numBuckets;
}

int HT_CreateFile(char *fileName,  int buckets){
  CALL_OR_DIE(BF_CreateFile(fileName));

  int file_desc;
  CALL_OR_DIE(BF_OpenFile(fileName, &file_desc));

  BF_Block* block;
  BF_Block_Init(&block);

  CALL_OR_DIE(BF_AllocateBlock(file_desc, block));

  void* data = BF_Block_GetData(block);
  HT_info* ht_info = data;

  ht_info->file_desc = file_desc;
  ht_info->numBuckets = buckets;
  ht_info->num_o_blocks = 1;
  ht_info->max_records = floor((BF_BLOCK_SIZE - sizeof(HT_block_info))/sizeof(Record)); 
  ht_info->hashtable[buckets];

  BF_Block_SetDirty(block);
  CALL_OR_DIE(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  return 0;
}

HT_info* HT_OpenFile(char *fileName){
  int file_desc, i;
  void* data;
  CALL_OR_DIE(BF_OpenFile(fileName, &file_desc));

  BF_Block* block0;
  BF_Block_Init(&block0);
  CALL_OR_DIE(BF_GetBlock(file_desc,0,block0));

  void* d = BF_Block_GetData(block0);
  //HT_info* ht_info = malloc(sizeof(HT_info)); //looks like there is a problem with malloc, it doesn't know how much memory to allocate for the hashtable 
  static HT_info ht_info;
  memcpy(&ht_info,d,sizeof(HT_info));

  int block_info_margin = ht_info.max_records*sizeof(Record);

  for(i = 0; i < ht_info.numBuckets; i++){
    BF_Block* block;
    BF_Block_Init(&block);
    CALL_OR_DIE(BF_AllocateBlock(file_desc, block));

    data = BF_Block_GetData(block);
    HT_block_info* block_info = data+block_info_margin;

    block_info->num_o_records = 0;
    block_info->bucket = i;
    block_info->previous_block = 0; //we'll treat zero as the block not having overflowed
    ht_info.hashtable[i] = i+1; // we add one since we can't have any bucket match with block0, this block is for our metadata only

    BF_Block_SetDirty(block);
    CALL_OR_DIE(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
  }
  memcpy(data,&ht_info,sizeof(HT_info));
  CALL_OR_DIE(BF_UnpinBlock(block0));
  BF_Block_Destroy(&block0);
  return &ht_info;
}


int HT_CloseFile( HT_info* ht_info ){
  CALL_OR_DIE(BF_CloseFile(ht_info->file_desc));
  //free(ht_info); 
  return 0;
}

int HT_InsertEntry(HT_info* ht_info, Record record){
  void* data;
  int block_id;
  int block_info_margin = ht_info->max_records*sizeof(Record);
  //printf("%d Loading block zero\n", __LINE__);
  BF_Block* block0;
  BF_Block_Init(&block0);
  CALL_OR_DIE(BF_GetBlock(ht_info->file_desc,0,block0));
  //printf("%d Getting metadata\n", __LINE__);
  void* d = BF_Block_GetData(block0);
  // HT_info* info = d; 
  //printf("%d Get data\n", __LINE__);

  int bucket = hash(record.id,ht_info);
  block_id = ht_info->hashtable[bucket]; //find the block for the specific id
  //printf("%d Block %d for bucket %d\n", __LINE__, block_id, bucket);

  BF_Block* block;
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_GetBlock(ht_info->file_desc,block_id,block));
  //printf("%d Loaded block X in memory\n", __LINE__);

  data = BF_Block_GetData(block);
  HT_block_info* block_info = data + block_info_margin;
  //printf("%d Got block's metadata\n", __LINE__);

  if(block_info->num_o_records < ht_info->max_records){
    int margin = block_info->num_o_records*sizeof(Record); 
    memcpy(data+margin, &record, sizeof(Record));
    //printf("%d Block has enoough space, copied record into block\n", __LINE__);

    block_info->num_o_records++;
    BF_Block_SetDirty(block);
    CALL_OR_DIE(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    //printf("%d Destroyed block X, exiting\n", __LINE__);
  }
  else{
    int new_block_id;
    CALL_OR_DIE(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    //printf("%d Not enough space, destroyed block X\n", __LINE__);

    BF_Block* new_block;
    BF_Block_Init(&new_block);
    CALL_OR_DIE(BF_AllocateBlock(ht_info->file_desc, new_block)); //allocate new block
    CALL_OR_DIE(BF_GetBlockCounter(ht_info->file_desc,&new_block_id));
    //printf("%d Allocated new block Y\n", __LINE__);

    void* new_data = BF_Block_GetData(new_block);
    HT_block_info* new_block_info = new_data + block_info_margin; 
    memcpy(new_data, &record, sizeof(Record));

    new_block_info->num_o_records=1;
    new_block_info->bucket = bucket;
    new_block_info->previous_block = block_id; // so we must somewhere save the previous block as well
    //printf("%d Copied the record and updated block's Y metadata\n", __LINE__);
    //the bucket now must try and fill the new block 
    ht_info->hashtable[bucket] = new_block_id-1;  //we subtract one because the block's id is one less than the number of blocks             
    ht_info->num_o_blocks++; //increment the number of blocks
    printf("%d New:%d Prev:%d \n", __LINE__,new_block_id,new_block_info->previous_block);
    BF_Block_SetDirty(new_block);
    CALL_OR_DIE(BF_UnpinBlock(new_block));
    BF_Block_Destroy(&new_block);
    //printf("%d Updated files metadata and destroyed block Y\n", __LINE__);
  } 

  memcpy(d,ht_info,sizeof(HT_info)); //update the internal ht_info structure with the resolution of the operation(The opposite was better for the heap since I didn't need to play with memory in between calls in the equivelent function)
  BF_Block_SetDirty(block0);
  CALL_OR_DIE(BF_UnpinBlock(block0));
  BF_Block_Destroy(&block0);
  //printf("%d Destroyed block zero, we are done here\n", __LINE__);

  return 0;
}

int HT_GetAllEntries(HT_info* ht_info, void *value ){
  void* d;
  int block_counter = 0;
  int block_info_margin = ht_info->max_records*sizeof(Record);
  int bucket = hash(*(int*)value,ht_info);
  int block_id = ht_info->hashtable[bucket];

  BF_Block* block;
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_GetBlock(ht_info->file_desc,block_id,block));
  void* data = BF_Block_GetData(block);
  HT_block_info* block_info = data + block_info_margin;

  //hash specific search to the block and all of its overflowed instances
  if(HT_GetRecordinBlock(ht_info, value, block_id)) block_counter++;

  int prev = block_info->previous_block;
  while(prev != 0){
    if(HT_GetRecordinBlock(ht_info, value, prev)) block_counter++;
    prev--;
  }

  if(block_counter!=0) return block_counter;
  else return -1;
}

bool HT_GetRecordinBlock(HT_info* ht_info, void *value, int block_id){
  int j;
  int block_info_margin = ht_info->max_records*sizeof(Record);
  bool flag = false;
  BF_Block* block;
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_GetBlock(ht_info->file_desc,block_id,block));

  void* data = BF_Block_GetData(block);
  HT_block_info* block_info = data + block_info_margin; //getting the block's metadata the good ol' way

  for(j = 0; j < block_info->num_o_records; j++){ //and for every record in this block
    Record* record = data + j*sizeof(Record); 
    if(record->id == *(int*)value){ 
      flag = true;
      printf("Id: %d\nName: %s\nSurname: %s\nCity: %s\n", record->id,record->name,record->surname,record->city);
    }
  }
  CALL_OR_DIE(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  return flag;
}


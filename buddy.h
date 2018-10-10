#ifndef buddy_h
#define buddy_h

#include <iostream>
#include <mutex>
#include "slab.h"

using namespace std;

typedef union BuddyBlock{
	union BuddyBlock* next;
	unsigned char data[BLOCK_SIZE];
}BuddyBlock;


typedef struct BuddyElem{
	BuddyBlock *first;
	BuddyBlock *last;
}BuddyElem;


typedef struct BuddyHead{
	BuddyElem elements[32];
	mutex mutex_space;
	mutex* lock;
	int blocknum;
	int blockused;
	int blocksize;
}BuddyHead;

BuddyHead* BuddyInit(void *space, int block_num);				//inicijalizuje buddy strukturu
void BuddyAdd(BuddyHead* bb, BuddyBlock* block, int blocknum);		//dodaje blokove kojih ima blocknum sa pocetnim blokom u block
int BuddyBreak(BuddyHead* bh, int brulaza); //deli jedan blok u ulazu brulaza, na dva manja i stavja ih u manji ulaz
											//vraca 1 ako je razbio, 0 ako nije
BuddyBlock* BuddyTake(BuddyHead *bh, int* numblocks);		//vraca poziciju prvog bloka, a u *numblocks vraca broj blokova koji je dodeljen
															//vraca NULL u slucaju greske
int BuddyCheck(BuddyHead* bh, int br);		//provera jel moguce uzeti dati broj blokova, 1-da 0-ne
void BuddyToString(BuddyHead* bh); //pise korisne info o buddy

#endif
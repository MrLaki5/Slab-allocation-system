#include <stdio.h>
#include "buddy.h"


void BuddyAdd_(BuddyHead* bb, BuddyBlock* block, int blocknum){
	int bsize = 0;
	int i = 1;
	for (bsize = 0; i < blocknum; bsize++){				//racunanje stepena dvojke broja blokova
		i *= 2;
	}
	BuddyBlock *pomb = bb->elements[bsize].first;
	BuddyBlock *preth = NULL;
	BuddyBlock *slanje = NULL;
	while (pomb!=NULL){
		if (((unsigned int)pomb >> ((bb->blocksize + bsize + 1))) == ((unsigned int)block >> ((bb->blocksize + bsize + 1)))){		//provera da li je najvisih par bita jednako u adresama, ako jeste, onda su to dva bloka koji su jedan pored drugog
			if (((unsigned int)pomb >> (bb->blocksize + bsize)) << (sizeof(unsigned int) * 8 - 1)){	//trazi se blok od dva uzajamna sa nizom adresom
				slanje = block;
			}
			else
				slanje = pomb;
			if (preth){							//prevezivanje
				preth->next = pomb->next;
			}
			else
				bb->elements[bsize].first=pomb->next;
			BuddyAdd_(bb, slanje, blocknum * 2);				//rekurzivno povezivanje funkcije sada za spojeni blok viseg reda
			return;
		}
		preth = pomb;
		pomb = pomb->next;
	}
	block->next = bb->elements[bsize].first;		//nema bloka para, pa ovaj samo treba uvezati u listu
	bb->elements[bsize].first = block;
}

void BuddyAdd(BuddyHead* bb, BuddyBlock* block, int blocknum){
	bb->lock->lock();
	BuddyAdd_(bb, block, blocknum);
	bb->lock->unlock();
}

int BuddyBreak(BuddyHead* bh, int brulaza){
	if (bh->elements[brulaza].first == NULL)			//provera da li ima elementa za deljenje
		return 0;
	if (brulaza == 0)				//provera da nije samo jedan blok ubacen
		return 0;
	BuddyBlock *bb = bh->elements[brulaza].first;
	bh->elements[brulaza].first = bb->next;
	bb->next = NULL;
	unsigned int a, b, c, d;
	a = (((unsigned int)bb) >> (bh->blocksize + brulaza ))<<1;
	b = (((unsigned int)bb) << (sizeof(unsigned int) * 8 - (bh->blocksize + brulaza-1))) >> (sizeof(unsigned int) * 8 - (bh->blocksize + brulaza-1));
	c = a << (bh->blocksize + brulaza-1);
	a = a + 1;
	d = a << (bh->blocksize + brulaza-1);			//secenje jednog skupa blokova na dva jednaka
	c = c + b;
	d = d + b;
	BuddyBlock *pa=(BuddyBlock*)c;
	BuddyBlock *pb=(BuddyBlock*)d;
	pa->next = NULL;
	pb->next = bh->elements[brulaza - 1].first;
	bh->elements[brulaza - 1].first = pb;	//dodavanje oba skupa nazad u buddy
	pa->next = bh->elements[brulaza - 1].first;
	bh->elements[brulaza - 1].first = pa;
	return 1;
}

BuddyBlock* BuddyTake(BuddyHead *bh, int *numblocks){
	int bsize = 0;
	int i = 1;
	BuddyBlock *temp = NULL;
	for (bsize = 0; i < *numblocks; bsize++){				//racunanje stepena dvojke broja blokova
		i *= 2;
	}
	*numblocks = i;
	if (bsize >= 32) return NULL;							//izvan opsega buddy sistema
	bh->lock->lock();
	for (i = bsize; i < 32; i++){
		if (bh->elements[i].first != NULL){
			while (i > bsize){
				BuddyBreak(bh, i);				
				i--;
			}
			temp = bh->elements[i].first;						//pronadjena je dovoljna kolicina memorije
			bh->elements[i].first = temp->next;
			temp->next = NULL;
			bh->lock->unlock();
			return temp;
		}
	}
	bh->lock->unlock();
	return NULL;				//nema dovoljno velika kolicina uzastopne memorije u buddy
}

int BuddyCheck(BuddyHead* bh, int br){
	int bsize = 0;
	int i = 1;
	for (bsize = 0; i < br; bsize++){				//racunanje stepena dvojke kolicine blokova
		i *= 2;
	}
	if (bsize >= 32)
		return 0; 
	bh->lock->lock();
	for (i = bsize; i < 32; i++){
		if (bh->elements[i].first != NULL){
			bh->lock->unlock();
			return 1;
		}
	}
	bh->lock->unlock();
	return 0;
}

BuddyHead* BuddyInit(void *space, int block_num){
	int bsize=0;
	int i = 1;
	for (bsize = 0; i < BLOCK_SIZE; bsize++){				//racunanje stepena dvojke velicine blokova
		i *= 2;
	}
	if (((unsigned int)space) << (sizeof(void*) * 8 - bsize)){		//siftovanje na pocetak bloka
		unsigned int pom1 = 0xffff<<bsize;
		space = (void*)((unsigned int)space &pom1);
		pom1 = 0x0001;
		pom1 = pom1 << bsize;
		space = (void*)((unsigned int)space + pom1);
		block_num--;
	}
	BuddyHead *bh = (BuddyHead*)space;					//inicijalizacija hedera za buddy
	bh->blocknum = 1;
	bh->blocksize = bsize;
	bh->blockused = 1;
	bh->lock = new((void*)(&bh->mutex_space)) std::mutex();
	for (i = 0; i < 32; i++){
		bh->elements[i].first = NULL;
		bh->elements[i].last = NULL;
	}
	BuddyBlock *bb = (BuddyBlock*)space;
	for (i = 1; i < block_num; i++){					//dodavanje praznih blokova u buddy
		bb++;
		bb->next = NULL;
		BuddyAdd(bh, bb, 1);
		bh->blocknum++;
	}
	return bh;
}

void BuddyToString(BuddyHead* bh){
	int i;
	BuddyBlock *bb;
	bh->lock->lock();
	printf("Buddy system: \n");
	printf("Number of blocks: %d\n", bh->blocknum);
	printf("Used blocks: %d\n", bh->blockused);
	printf("Structure: \n");
	for (i = 0; i < 32; i++){
		bb = bh->elements[i].first;
		printf("%d: ", i);
		while (bb){
			printf("%x ", bb);
			bb = bb->next;
		}
		printf("\n");
	}
	bh->lock->unlock();
}


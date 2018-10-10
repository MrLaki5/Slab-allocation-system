#ifndef cash_h
#define cash_h

#include "buddy.h"

typedef struct CashHeadExtern CashHeadExtern;

typedef struct SlabHead{
	struct SlabHead* next;						//sledeci slab u nizu
	unsigned int colouroff;						//pomeraj zbog L1 poravnanja
	void* startaddr;							//pocetna adresa prvog objekta
	unsigned int indexfree;						//broj prvog slobodnog objekta
	unsigned int number_of_objects;				//broj koriscenih objekata
	kmem_cache_t* cash;							//pokazivac na kes u kojem se slab nalazi
}SlabHead;

typedef struct kmem_cache_s{
	struct kmem_cache_s* next;					//sledeci kes
	CashHeadExtern* che;						//povratni pokazivac na strukturu koja pamti gde su kesevi
	SlabHead* prazni;							//lista praznih slabova
	SlabHead* puni;								//lista punih slabova
	SlabHead* polu;								//lista polu punih slabova
	size_t objectsize;							//velicina objekta
	char name[32];								//ime kesa
	unsigned int slab_size;						//broj blokova po slabu
	unsigned int number_of_slabs;				//broj slabova u kesu
	unsigned int number_of_objects;				//broj objekata u upotrebi
	unsigned int niz_slab_free_elem_max_num;	//velicina unsigned int niza iza slabhead-a 
	unsigned int max_objects_num;				//maximalni broj objekata
	int ready_free;								//moze da ukloni prazne slabove, 1-da 0-ne
	void(*ctor)(void *);						//konstruktor
	void(*dtor)(void *);						//destruktor 
	unsigned int colour;						//max pomeraj za L1 cache
	unsigned int colour_next;					//pomeraj za sledeci slab, kad se doda
	unsigned int error;							//error flags
	mutex mutex_place;							//pozicija za cuvanje semafora
	mutex* lock;								//pokazivac na semafor
}Cash;

struct CashHeadExtern{
	struct CashHeadExtern* next;				//sledeci desktriptor u nizu
	unsigned int freeslots;						//broj slobodnih mesta u blokovima koje desktriptor zastupa
	unsigned int allslots;						//broj svih mesta u blokovima koje deskriptor zastupa
	Cash* freecashes;							//ulancana lista slobodnih keseva u tom desktriptoru, NULL ako ih nema
	unsigned int structblockusage;				//broj blokova koje deskriptor zastupa
};

typedef struct CashHeadMain{
	Cash *buffcash[13];							//pokazivaci na keseve za male bafere, u pocetku NULL
	Cash *objectcash;							//lista inicijalizovanih keseva, bez malih bafera
	CashHeadExtern *first;						//pokazivac na prvi deskriptor
	BuddyHead *Buddy;							//pokazivac na buddy menadzera
	unsigned int structblockusage;				//broj blokova koji je alociran za ovu strukturu, iza nje u istom delu se nalazi
												//i prvi CashHeadExtern koji sluzi za smestanje keseva
	mutex mutex_place;							//mesto za cuvanje semafora
	mutex *lock;								//pokazivac na semafor
}CashHeadMain;

Cash* findCashSpot();							//trazenje mesta za smestanje deskriptora kesa
CashHeadMain* initCashHeadMain(BuddyHead* bh);	//inicijalizovanje strukture za cuvanje kesa
CashHeadExtern* initCashHeadExtern(void* poc, void* kr, int blocknum);		//inicijalizovanje pomocnih struktura za cuvanje kesa



void kmem_init_(void *space, int block_num);
kmem_cache_t *kmem_cache_create_(const char *name, size_t size, void(*ctor)(void *), void(*dtor)(void *));
int kmem_cache_shrink_(kmem_cache_t *cachep);
void *kmem_cache_alloc_(kmem_cache_t *cachep);
void kmem_cache_free_(kmem_cache_t *cachep, void *objp);
void *kmalloc_(size_t size);
void kfree_(const void *objp);
void kmem_cache_destroy_(kmem_cache_t *cachep);
void kmem_cache_info_(kmem_cache_t *cachep);
int kmem_cache_error_(kmem_cache_t *cachep);




#endif
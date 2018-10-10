#include "cash.h"
mutex wrt;
static CashHeadMain *schm = NULL;

CashHeadExtern* initCashHeadExtern(void* poc, void* kr, int blocknum){
	CashHeadExtern *che = (CashHeadExtern*)poc;
	che->freeslots = ((int)kr - ((int)poc+sizeof(CashHeadExtern))) / sizeof(Cash);
	che->allslots = che->freeslots;
	che->freecashes = NULL;
	che->next = NULL;
	che->structblockusage = blocknum;
	int i = 0;
	Cash *ca = (Cash*)((unsigned int)poc + sizeof(CashHeadExtern));
	for (i = 0; i < che->freeslots; i++){
		ca->next = che->freecashes;
		che->freecashes = ca;
		ca->che = che;
		ca++;
	}
	return che;
}

CashHeadMain* initCashHeadMain(BuddyHead* bh){
	int broj = 8;
	BuddyBlock* bb = BuddyTake(bh, &broj);
	if (bb == NULL){
		return NULL;
	}
	CashHeadMain *chm = (CashHeadMain*)bb;
	chm->Buddy = bh;
	int i = 0;
	for (i = 0; i < 13; i++){
		chm->buffcash[i] = NULL;
	}
	chm->objectcash = NULL;
	chm->structblockusage = broj;
	chm->lock = new((void*)(&chm->mutex_place)) std::mutex();
	void* poc = (void*)((unsigned int)(bb)+sizeof(CashHeadMain));
	void* kr = (void*)((unsigned int)bb+broj*BLOCK_SIZE-1);						
	chm->first = initCashHeadExtern(poc, kr, broj);
	return chm;
}

Cash* findCashSpot(){
	if (schm == NULL) return NULL;
	Cash* temp;
	CashHeadExtern* che = schm->first;
	while (che){
		if (che->freeslots){
			temp = che->freecashes;
			che->freecashes = temp->next;
			temp->next = NULL;
			che->freeslots--;
			return temp;
		}
		che = che->next;
	}
	che = schm->first;
	while (che->next){
		che = che->next;
	}
	int broj = 8;
	BuddyBlock *bb = BuddyTake(schm->Buddy, &broj);
	if (bb == NULL) return NULL;
	void* poc = (void*)((unsigned int)(bb));
	void* kr = (void*)((unsigned int)bb + broj*BLOCK_SIZE-1);		
	che->next = initCashHeadExtern(poc, kr, broj);
	che = che->next;
	temp = che->freecashes;
	che->freecashes = temp->next;
	temp->next = NULL;
	che->freeslots--;
	return temp;
}

void kmem_init_(void *space, int block_num){
	if (schm != NULL) return;
	BuddyHead *bh = BuddyInit(space, block_num);
	if (bh == NULL){
		wrt.lock();
		cout<<"Error in buddy creation, kmem_init/BuddyInit return NULL\n";
		wrt.unlock();
		return;
	}
	CashHeadMain *chm = initCashHeadMain(bh);
	if (chm == NULL){
		wrt.lock();
		cout<<"Error in creation of cash structure, kmem_init/initCashHeadMain return NULL\n";
		wrt.unlock();
		return;
	}
	schm = chm;
}

int SlabSizeCalculator_(BuddyHead* bh, size_t objsize, unsigned int *zaniz){
	int i = 1, ip=0;
	int flag = BuddyCheck(bh,i);
	if (flag == 0) return 0;
	float r = 0, ri = 0;
	unsigned int furniz;
	while (flag){
		r = (float)i*BLOCK_SIZE-sizeof(SlabHead);
		furniz = (unsigned int)r / (objsize+sizeof(void*));
		r = r - furniz*sizeof(unsigned int);
		r = (unsigned int)r / (objsize+sizeof(void*));
		r = (r*objsize);
		r = (r * 100) / ((float)i*BLOCK_SIZE - (sizeof(SlabHead) + furniz*sizeof(unsigned int)));
		if (r > ri){
			ri = r;
			ip = i;
			*zaniz = furniz;
			if (r >= 90){
				return ip;
			}
		}
		i *= 2;
		flag = BuddyCheck(bh, i);
	}
	return ip;
}

int SlabSizeCalculator(BuddyHead* bh, size_t objsize, unsigned int *zaniz){
	int i = 1;
	unsigned int k = BLOCK_SIZE;
	for (i = 1; k < (sizeof(void*)+objsize+sizeof(SlabHead)+(i*BLOCK_SIZE/(objsize+sizeof(void*))));){
		k = k*2;
		i=i*2;
	}
	int flag = BuddyCheck(bh, i);
	*zaniz = i*BLOCK_SIZE / (objsize + sizeof(void*));
	if (flag == 0) return 0;
	return i;
}

SlabHead* SlabCreate(BuddyHead *bh, Cash* cash){
	if (cash->slab_size == 0){
		unsigned int niz_u_slabu = 0;
		int slab_size = SlabSizeCalculator(schm->Buddy, cash->objectsize, &niz_u_slabu);
		if (slab_size == 0){
			if ((cash->error&1)==0)
				cash->error += 1;
			return NULL;
		}
		cash->slab_size = slab_size;
		cash->niz_slab_free_elem_max_num = niz_u_slabu;
		unsigned int f = cash->slab_size*BLOCK_SIZE - (sizeof(SlabHead) + sizeof(unsigned int)*cash->niz_slab_free_elem_max_num);
		f = (unsigned int)f / (cash->objectsize + sizeof(void*));
		cash->max_objects_num = (unsigned int)f;
		f = (cash->slab_size*BLOCK_SIZE - (sizeof(SlabHead) + sizeof(unsigned int)*cash->niz_slab_free_elem_max_num)) - f*(cash->objectsize + sizeof(void*));
		cash->colour = (unsigned int)f / CACHE_L1_LINE_SIZE;
		cash->colour_next = 0;
		cash->number_of_slabs = 0;
		cash->number_of_objects = 0;
	}
	BuddyBlock *bb = BuddyTake(bh, (int*)(&cash->slab_size));
	if (bb == NULL){
		if ((cash->error&2)==0)
			cash->error += 2;
		return NULL;
	}
	SlabHead *sh = (SlabHead*)bb;
	sh->cash = cash;
	sh->next = cash->prazni;
	cash->prazni = sh;
	sh->indexfree = 0;
	sh->startaddr = (void*)((unsigned int)sh + sizeof(SlabHead) + sizeof(unsigned int)*cash->niz_slab_free_elem_max_num+cash->colour_next*CACHE_L1_LINE_SIZE);
	sh->colouroff = cash->colour_next;
	sh->number_of_objects = 0;
	cash->colour_next++;
	cash->number_of_slabs++;
	cash->ready_free = 0;
	if (cash->colour_next > cash->colour){
		cash->colour_next = 0;
	}
	unsigned int *elems = (unsigned int *)((unsigned int)sh + sizeof(SlabHead));
	unsigned int i = 0;
	void* pom = sh->startaddr;
	for (i = 0; i < cash->max_objects_num; i++){
		elems[i] = i + 1;
		if (cash->ctor != NULL){
			(*(cash->ctor))(pom);										
		}
		pom = (void*)((unsigned int)pom + cash->objectsize);
		*((SlabHead**)pom) = sh;
		pom = (void*)((unsigned int)pom + sizeof(void*));
	}
	return sh;
}

kmem_cache_t *kmem_cache_create__(const char *name, size_t size, void(*ctor)(void *), void(*dtor)(void *)){
	Cash *cash = findCashSpot();
	if (cash == NULL){
		wrt.lock();
		cout << "error in finding spot for cache, kmem_ceche_create/findCashSpot return NULL\n";
		wrt.unlock();
		return NULL;
	}
	int i = 0;
	for (i = 0; name[i] != '\0'; i++){
		cash->name[i] = name[i];
	}
	cash->name[i] = '\0';
	cash->objectsize = size;
	cash->ctor = ctor;
	cash->dtor = dtor;
	cash->polu = NULL;
	cash->prazni = NULL;
	cash->puni = NULL;
	cash->slab_size = 0;
	cash->next = schm->objectcash;
	cash->error = 0;
	cash->lock = new ((void*)(&cash->mutex_place))std::mutex();
	schm->objectcash = cash;
	SlabHead *sh=SlabCreate(schm->Buddy, cash);
	if (sh == NULL){
		wrt.lock();
		cout << "error in creating first slab, kmem_ceche_create/SlabCreate return NULL";
		wrt.unlock();
		return NULL;
	}
	return cash;
}

kmem_cache_t *kmem_cache_create_(const char *name, size_t size, void(*ctor)(void *), void(*dtor)(void *)){
	schm->lock->lock();
	kmem_cache_t* pom=kmem_cache_create__(name, size, ctor, dtor);
	schm->lock->unlock();
	return pom;
}

void *kmem_cache_alloc_(kmem_cache_t *cash){
	cash->lock->lock();
	if (cash->polu != NULL){
		void* temp = (void*)((unsigned int)cash->polu->startaddr + cash->polu->indexfree*(sizeof(void*) + cash->objectsize));
		cash->number_of_objects++;
		cash->polu->number_of_objects++;
		unsigned int *elements = (unsigned int*)((unsigned int)cash->polu + sizeof(SlabHead));
		cash->polu->indexfree = elements[cash->polu->indexfree];
		if (cash->max_objects_num == cash->polu->number_of_objects){
			SlabHead* ch = cash->polu;
			cash->polu = ch->next;
			ch->next = cash->puni;
			cash->puni = ch;
		}
		cash->lock->unlock();
		return temp;
	}
	if (cash->prazni == NULL){
		SlabHead* sh=SlabCreate(schm->Buddy, cash);
		if (sh == NULL){
			if ((cash->error&4)==0)
				cash->error += 4;
			cash->lock->unlock();
			return NULL;
		}
	}
	void* temp = (void*)((unsigned int)cash->prazni->startaddr + cash->prazni->indexfree*(sizeof(void*) + cash->objectsize));
	cash->number_of_objects++;
	cash->prazni->number_of_objects++;
	unsigned int *elements = (unsigned int*)((unsigned int)cash->prazni + sizeof(SlabHead));
	cash->prazni->indexfree = elements[cash->prazni->indexfree];
	SlabHead* ch = cash->prazni;
	cash->prazni = ch->next;
	if (ch->number_of_objects == cash->max_objects_num){
		ch->next = cash->puni;
		cash->puni = ch;
	}
	else{
		ch->next = cash->polu;
		cash->polu = ch;
	}
	cash->lock->unlock();
	return temp;
}

void kmem_cache_free__(kmem_cache_t *cash, void *objp){
	if (cash->dtor != NULL)
		(*(cash->dtor))(objp);
	if (cash->ctor != NULL)
		(*(cash->ctor))(objp);
	SlabHead* sh = *(SlabHead**)((unsigned int)objp + cash->objectsize);
	if (sh->number_of_objects == 0) return;
	unsigned int objnum = ((unsigned int)objp - (unsigned int)sh->startaddr) / (cash->objectsize+sizeof(void*));
	unsigned int *elements = (unsigned int*)((unsigned int)sh + sizeof(SlabHead));
	elements[objnum] = sh->indexfree;
	sh->indexfree = objnum;
	cash->number_of_objects--;
	if ((sh->number_of_objects == cash->max_objects_num)&&(cash->puni)){
		SlabHead* pom = cash->puni;
		SlabHead* preth=NULL;
		while (pom != sh){
			preth = pom;
			pom = pom->next;
		}
		if (preth == NULL){
			cash->puni = pom->next;
		}
		else{
			preth->next = pom->next;
		}
		pom->next = cash->polu;
		cash->polu = pom;
	}
	if (sh->number_of_objects == 1){
		SlabHead* pom=cash->polu;
		SlabHead* preth=NULL;
		while (pom != sh){
			preth = pom;
			pom = pom->next;
		}
		if (preth == NULL){
			cash->polu = pom->next;
		}
		else{
			preth->next = pom->next;
		}
		pom->next = cash->prazni;
		cash->prazni = pom;
		cash->ready_free = 1;
	}
	sh->number_of_objects--;
}

void kmem_cache_free_(kmem_cache_t *cash, void *objp){
	cash->lock->lock();
	kmem_cache_free__(cash, objp);
	cash->lock->unlock();
}

void kmem_cache_destroy__(kmem_cache_t *cash){
	SlabHead* pom;
	while (cash->puni){
		pom = cash->puni;
		cash->puni = pom->next;
		BuddyAdd(schm->Buddy, (BuddyBlock*)pom, cash->slab_size);
	}
	while (cash->polu){
		pom = cash->polu;
		cash->polu = pom->next;
		BuddyAdd(schm->Buddy, (BuddyBlock*)pom, cash->slab_size);
	}
	while (cash->prazni){
		pom = cash->prazni;
		cash->prazni = pom->next;
		BuddyAdd(schm->Buddy, (BuddyBlock*)pom, cash->slab_size);
	}
	Cash *pret = NULL;
	Cash* tren = schm->objectcash;
	while ((tren!=cash)&&(tren!=NULL)){
		pret = tren;
		tren = tren->next;
	}
	if (tren == NULL){
		unsigned int i;
		for (i = 0; i < 13; i++){
			if (schm->buffcash[i] == cash){
				schm->buffcash[i] = NULL;
				break;
			}
		}
		if (i == 13){
			wrt.lock();
			cout<<"Unknown cache\n";
			wrt.unlock();
			return;
		}
	}
	else{
		if (pret == NULL){
			schm->objectcash = cash->next;
		}
		else{
			pret->next = cash->next;
		}
	}
	cash->next = cash->che->freecashes;
	cash->che->freeslots++;
	cash->che->freecashes = cash;
	if ((cash->che->freeslots==cash->che->allslots) && (schm->first!=cash->che)){
		CashHeadExtern *cpret = NULL;
		CashHeadExtern *ctren = schm->first;
		while (ctren != cash->che){
			cpret = ctren;
			ctren = ctren->next;
		}
		cpret->next = ctren->next;
		BuddyAdd(schm->Buddy, (BuddyBlock*)ctren, ctren->structblockusage);
	}
}

void kmem_cache_destroy_(kmem_cache_t *cash){
	schm->lock->lock();
	cash->lock->lock();
	kmem_cache_destroy__(cash);
	cash->lock->unlock();
	schm->lock->unlock();
}

void *kmalloc_(size_t size){
	unsigned int bsize = 0;
	unsigned int i = 1;
	for (bsize = 0; i < size; bsize++){			
		i *= 2;
	}
	if ((bsize < 5) || (bsize>17)){
		wrt.lock();
		cout << "Size of small buffer is not regular\n";
		wrt.unlock();
		return NULL;
	}
	schm->lock->lock();
	if (schm->buffcash[bsize - 5] == NULL){
		char* pom = "size-";
		char c[10];
		for (i = 0; pom[i] != '\0'; i++){
			c[i] = pom[i];
		}
		if (bsize >= 10){
			c[i] = 49;
			c[i + 1] = 48 + bsize - 10;
			c[i + 2] = '\0';
		}
		else{
			c[i] = 48 + bsize;
			c[i + 1] = '\0';
		}
		schm->buffcash[bsize - 5] = kmem_cache_create__(c, size, NULL, NULL);
		if (schm->buffcash[bsize - 5]==NULL){
			schm->lock->unlock();
			wrt.lock();
			cout << "Error in creating cache for small buffer, kmalloc/kmem_ceche_create return NULL\n";
			wrt.unlock();
			return NULL;
		}
		schm->objectcash = schm->objectcash->next;
		schm->buffcash[bsize - 5]->next = NULL;
	}
	void* pomp=kmem_cache_alloc_(schm->buffcash[bsize - 5]);
	schm->lock->unlock();
	return pomp;																				
}

void kfree_(const void *objp){
	unsigned int i;
	Cash* temp = NULL;
	schm->lock->lock();
	for (i = 0; i < 13; i++){
		if (schm->buffcash[i] != NULL){
			Cash* pom = schm->buffcash[i];
			pom->lock->lock();
			SlabHead* sh = pom->polu;
			while (sh){
				if (((unsigned int)sh<(unsigned int)objp) && (((unsigned int)sh + pom->slab_size*BLOCK_SIZE)>(unsigned int)objp)){
					temp = pom;
					break;
				}
				sh = sh->next;
			}
			if (temp != NULL) break;
			sh = pom->puni;
			while (sh){
				if (((unsigned int)sh<(unsigned int)objp) && (((unsigned int)sh + pom->slab_size*BLOCK_SIZE)>(unsigned int)objp)){
					temp = pom;
					break;
				}
				sh = sh->next;
			}
			if (temp != NULL) break;
			pom->lock->unlock();
		}
	}
	if (temp == NULL){
		schm->lock->unlock();
		wrt.lock();
		cout<<"Unknown object\n";
		wrt.unlock();
		return;
	}
	kmem_cache_free__(temp, (void*)objp);
	if (temp->number_of_objects == 0){
		kmem_cache_destroy__(temp);
	}
	temp->lock->unlock();
	schm->lock->unlock();
}

int kmem_cache_shrink_(kmem_cache_t *cash){
	int temp = 0;
	cash->lock->lock();
	if ((cash->ready_free == 1) && (cash->prazni)){
		SlabHead* sh = cash->prazni;
		cash->prazni = NULL;
		while (sh){
			SlabHead* pom = sh;
			sh = sh->next;
			BuddyAdd(schm->Buddy,(BuddyBlock*)pom, cash->slab_size);
			cash->number_of_slabs--;
			temp += cash->slab_size;
		}
	}
	cash->lock->unlock();
	return temp;
}

void kmem_cache_info_(kmem_cache_t *cash){
	cash->lock->lock();
	wrt.lock();
	cout << "---------------------\n";
	cout<<"Name: "<<cash->name<<"\n";
	cout << "Object size: "<<cash->objectsize<<"B\n";
	cout << "Number of blocks: " << cash->number_of_slabs*cash->slab_size << "\n";
	cout << "Number of slabs: " << cash->number_of_slabs << "\n";
	//printf("Number of object per slab: %d\n", cash->number_of_objects / cash->number_of_slabs);
	cout << "Max number of objects per slab: " << cash->max_objects_num << "\n";
	cout << "Number of objects in cache: " << cash->number_of_objects << "\n";
	float f = (((float)cash->number_of_objects) / (cash->number_of_slabs*cash->max_objects_num)) * 100;
	cout << "Cache usage: " << f << "%\n";
	wrt.unlock();
	cash->lock->unlock();
}

int kmem_cache_error_(kmem_cache_t *cash){
	cash->lock->lock();
	if (cash->error){
		wrt.lock();
		if (cash->error & 1)
			cout << "error in calculating slab size, SlabCreate/SlabSizeCalculator return NULL\n";
		if (cash->error & 2)
			cout << "error in geting free place for slab, SlabCreate/BuddyTake return NULL\n";
		if (cash->error & 4)
			cout << "error in allocating new slab, kmem_cache_alloc/SlabCreate return NULL\n";
		wrt.unlock();
		cash->error = 0;
		cash->lock->unlock();
		return 1;
	}
	cash->lock->unlock();
	return 0;
}
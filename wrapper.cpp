#include "cash.h"


void kmem_init(void *space, int block_num){
	return kmem_init_(space, block_num);
}
kmem_cache_t *kmem_cache_create(const char *name, size_t size, void(*ctor)(void *), void(*dtor)(void *)){
	return kmem_cache_create_(name, size, ctor, dtor);
}
int kmem_cache_shrink(kmem_cache_t *cachep){
	return kmem_cache_shrink_(cachep);
}
void *kmem_cache_alloc(kmem_cache_t *cachep){
	return kmem_cache_alloc_(cachep);
}
void kmem_cache_free(kmem_cache_t *cachep, void *objp){
	return kmem_cache_free_(cachep, objp);
}
void *kmalloc(size_t size){
	return kmalloc_(size);

}
void kfree(const void *objp){
	return kfree_(objp);
}
void kmem_cache_destroy(kmem_cache_t *cachep){
	return kmem_cache_destroy_(cachep);
}
void kmem_cache_info(kmem_cache_t *cachep){
	return kmem_cache_info_(cachep);
}
int kmem_cache_error(kmem_cache_t *cachep){
	return kmem_cache_error_(cachep);
}
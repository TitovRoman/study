#include <iostream>
using namespace std;

void *alloc_slab(int order){
    int size = 1<<(12+order);
    return aligned_alloc(size, size);
}

void free_slab(void *slab){
    free(slab);
}

struct list;
struct block
{
    block* next_block;
};

/**
 * Структура хэдера SLABа
 * Хрониться в конце SLABа
 **/
struct header
{
    void* slab; /* указатель на начало SLAB */
    block* head; /* указатель на первый незанятый блок */
    header* prev; /* предыдущий header в списке */
    header* next; /* следующий header в списке */
    list* lst; /* текущий список SLABа */
    int cnt; /* количество незанятых блоков в SLAB */
};
struct list
{
    header* head;
};

void print_head(header* h);

/**
 * Структура аллокатора
 **/
struct cache {
    list free; /* список пустых SLAB-ов для поддержки cache_shrink */
    list part; /* список частично занятых SLAB-ов */
    list buzy; /* список заполненых SLAB-ов */
    
    size_t object_size; /* размер аллоцируемого объекта */
    int slab_order; /* order используемого размер SLAB-а */
    int slab_size; /* используемый размер SLAB-а */
    size_t slab_objects; /* количество объектов в одном SLAB-е */ 
};

int MIN_SLAB_OBJECTS = 63;
int HEADER_SIZE = sizeof(header);

header* get_header_from_slab(void* slab, int slab_size) 
{
    return reinterpret_cast<header*>(static_cast<char*>(slab) + slab_size - sizeof(header));
}

void remove_slab_from_list(header* header_slab)
{
    if(header_slab->lst != nullptr)
    {
        if(header_slab->prev == header_slab)
        {
            header_slab->lst->head = nullptr;
        }
        else {
            header_slab->next->prev = header_slab->prev;
            header_slab->prev->next = header_slab->next;
            if(header_slab->lst->head == header_slab)
            {
                header_slab->lst->head = header_slab->next;
            }
        }
    }
}

void insert_into_list(header* header_slab, list* lst)
{
    if(header_slab->lst == lst)
    {
        return;
    }
    
    remove_slab_from_list(header_slab);

    if(lst->head != nullptr)
    {
        header_slab->next = lst->head;
        header_slab->prev = lst->head->prev;

        lst->head->prev->next = header_slab;
        lst->head->prev = header_slab;
    }
    else {
        header_slab->prev = header_slab;
        header_slab->next = header_slab;
    }
    lst->head = header_slab;
    header_slab->lst = lst;
    
}

void* alloc_new_slab(struct cache *cache)
{
    void* slab = alloc_slab(cache->slab_order);

    header* header_slab = get_header_from_slab(slab, cache->slab_size);

    header_slab->slab = slab;
    header_slab->head = reinterpret_cast<block*>(slab);
    header_slab->cnt = cache->slab_objects;
    header_slab->next = nullptr;
    header_slab->prev = nullptr;
    header_slab->lst = nullptr;

    char* ptr = reinterpret_cast<char*>(slab);
    block* current = reinterpret_cast<block*>(ptr);

    for(int i = 0; i < cache->slab_objects - 1; ++i)
    {
        ptr += cache->object_size;
        current->next_block = reinterpret_cast<block*>(ptr);
        current = current->next_block;
    }
    current->next_block = nullptr;

    insert_into_list(header_slab, &cache->free);

    return slab;
}

void* get_mem_from_slab(header* header_slab)
{
        --(header_slab->cnt);
        void* result = static_cast<void*>(header_slab->head);
        header_slab->head = header_slab->head->next_block;

        return result;
}


/**
 * Функция инициализации будет вызвана перед тем, как
 * использовать это кеширующий аллокатор для аллокации.
 * Параметры:
 *  - cache - структура, которую вы должны инициализировать
 *  - object_size - размер объектов, которые должен
 *    аллоцировать этот кеширующий аллокатор 
 **/
void cache_setup(struct cache *cache, size_t object_size)
{
    int slab_objects;

    int min_object_size = sizeof(block) < object_size ? object_size : sizeof(block);

    int order = 0;
    int current_size = 4096;
    while((current_size - HEADER_SIZE) / min_object_size  < MIN_SLAB_OBJECTS){
        ++order;
        current_size *= 2;
    }
    
    cache->object_size = min_object_size;
    cache->slab_order = order;
    cache->slab_size = current_size;
    cache->slab_objects = int((current_size - HEADER_SIZE) / min_object_size);
    cache->free.head = nullptr;
    cache->part.head = nullptr;
    cache->buzy.head = nullptr;
}

/**
 * Функция освобождения всех SLAB из переданного списка.
 **/
void release_list(list* lst)
{
    header* header_slab;
    while((header_slab = lst->head) != nullptr)
    {
        remove_slab_from_list(header_slab);
        free_slab(header_slab->slab);
    }
}


/**
 * Функция освобождения будет вызвана когда работа с
 * аллокатором будет закончена. Она должна освободить
 * всю память занятую аллокатором. Проверяющая система
 * будет считать ошибкой, если не вся память будет
 * освбождена.
 **/
void cache_release(struct cache *cache)
{
    release_list(&cache->free);
    release_list(&cache->part);
    release_list(&cache->buzy);
}


/**
 * Функция должна освободить все SLAB, которые не содержат
 * занятых объектов. Если SLAB не использовался для аллокации
 * объектов (например, если вы выделяли с помощью alloc_slab
 * память для внутренних нужд вашего алгоритма), то освбождать
 * его не обязательно.
 **/
void cache_shrink(struct cache *cache)
{
    release_list(&cache->free);
}


/**
 * Функция аллокации памяти из кеширующего аллокатора.
 * Должна возвращать указатель на участок памяти размера
 * как минимум object_size байт (см cache_setup).
 * Гарантируется, что cache указывает на корректный
 * инициализированный аллокатор.
 **/
void *cache_alloc(struct cache *cache)
{    
    header* header_slab = nullptr;
    void* result = nullptr;
    if(cache->part.head != nullptr) 
    {
        header_slab = cache->part.head;

        result = get_mem_from_slab(header_slab);

        if(header_slab->cnt == 0) {
            insert_into_list(header_slab, &cache->buzy);
        }

    } else
    {
        if(cache->free.head != nullptr) 
        {
            header_slab = cache->free.head;   
        } else {
            header_slab = get_header_from_slab(alloc_new_slab(cache), cache->slab_size);
        }
        
        result = get_mem_from_slab(header_slab);
        insert_into_list(header_slab, &cache->part);
    }

    return result;
}


/**
 * Функция освобождения памяти назад в кеширующий аллокатор.
 * Гарантируется, что ptr - указатель ранее возвращенный из
 * cache_alloc.
 **/
void cache_free(struct cache *cache, void *ptr)
{
    void* slab = reinterpret_cast<void*>(reinterpret_cast<intptr_t>(ptr) & ~(cache->slab_size - 1));
    
    header* header_slab = get_header_from_slab(slab, cache->slab_size);
    block* block_ptr = reinterpret_cast<block*>(ptr);
    block_ptr->next_block = header_slab->head;
    header_slab->head = block_ptr;

    ++header_slab->cnt;
    if(header_slab->cnt == cache->slab_objects)
    {
        insert_into_list(header_slab, &cache->free);
    }
    else if(header_slab->cnt == 1)
    {
        insert_into_list(header_slab, &cache->part);
    }
}


void print_tree(list* lst)
{
    header* head = lst->head;
    header* current = lst->head;
    
    while (current != nullptr)
    {
        cout << current << " ---> "; 
        current = current->next;
        if(current == head) {
            break;
        }
    }
    
}

void print_head(header* h)
{
    cout << "Header!\nSelf = " << h << "; lst = " << h->lst <<"; next = " << h->next << "; Prev = " << h->prev << endl;
}

void print_cache(cache* my_cache){
    cout << endl;
    cout << "Lists heads." << endl;
    cout << "\tFree: ";
    print_tree(&my_cache->free);
    cout << "\n\tPart: ";
    print_tree(&my_cache->part);
    cout << "\n\tBuzy: ";
    print_tree(&my_cache->buzy);
    cout << "\nSlab size: " << my_cache->slab_size << endl;
    cout << "Slab objects: " << my_cache->slab_objects << endl;
    cout << endl;
}

void test() 
{
    cache* my_cache = new cache;
    cache_setup(my_cache, 1000);
    
    void* ptr = cache_alloc(my_cache);
    void* ptr2 = cache_alloc(my_cache);
    void* ptr3 = cache_alloc(my_cache);
    void* ptr4 = cache_alloc(my_cache);
        print_cache(my_cache);
    void* ptr5 = cache_alloc(my_cache);
    void* ptr6 = cache_alloc(my_cache);
    void* ptr7 = cache_alloc(my_cache);
    void* ptr8 = cache_alloc(my_cache);
    print_cache(my_cache);
    void* ptr11 = cache_alloc(my_cache);
    void* ptr12 = cache_alloc(my_cache);
    void* ptr13 = cache_alloc(my_cache);
    void* ptr14 = cache_alloc(my_cache);
    print_cache(my_cache);


    // cache_free(my_cache, ptr);
    cache_free(my_cache, ptr5);
    print_cache(my_cache);
    cache_free(my_cache, ptr3);
    print_cache(my_cache);
    ptr5 = cache_alloc(my_cache);
    print_cache(my_cache);
}

int main() {
    test();

    return 0;
}
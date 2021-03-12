struct Tag
{
    size_t size;
    bool free_flag;

    void init(size_t size, bool free_flag)
    {
        this->size = size;
        this->free_flag = free_flag;
    }
};

struct Lst
{
    Tag tag;
    Lst* next;
    Lst* prev;

    void init(Lst* next, Lst* prev, size_t size, bool free_flag)
    {
        this->next = next;
        this->prev = prev;
        this->tag.init(size, free_flag);
    }
};

void* g_buf;
Lst* head;
size_t g_size;
size_t tag_size = sizeof(Tag);
size_t lst_size = sizeof(Lst);

void init_free(void* ptr, Lst* next, Lst* prev, size_t size) 
{
    Lst* left_border = (Lst*)ptr;
    left_border->init(next, prev, size, true);

    Tag* right_border = (Tag*)((char*)ptr + size - tag_size);
    right_border->init(size, true);
}

void init_busy(void* ptr, size_t size) {
    Tag* left_border = (Tag*)ptr;
    left_border->init(size, false);

    Tag* right_border = (Tag*)((char*)ptr + size - tag_size);
    right_border->init(size, false);
}

// Эта функция будет вызвана перед тем как вызывать myalloc и myfree
    // используйте ее чтобы инициализировать ваш аллокатор перед началом
    // работы.
    //
    // buf - указатель на участок логической памяти, который ваш аллокатор
    //       должен распределять, все возвращаемые указатели должны быть
    //       либо равны NULL, либо быть из этого участка памяти
    // size - размер участка памяти, на который указывает buf
void mysetup(void* l_buf, size_t l_size)
{
    init_free(l_buf, nullptr, nullptr, l_size);
    head = (Lst*)l_buf;
    g_buf = l_buf;
    g_size = l_size;
}

void remove_lst(Lst* lst) {
    if (lst->prev == nullptr && lst->next == nullptr) {
        head = nullptr;
    }else
    {
        if (lst->prev != nullptr) {
        lst->prev->next = lst->next;
        }
        if (lst->next != nullptr) {
            lst->next->prev = lst->prev;
        }
        if (lst == head) {
            head = lst->next;
        }
    }
    
}

void* _myalloc(Lst* lst, size_t size) {
    void* ptr_result = nullptr;
    if (lst->tag.size - size <= tag_size + lst_size) {
        remove_lst(lst);
        ptr_result = (void*)lst;
        init_busy(ptr_result, lst->tag.size);
    }
    else {
        ptr_result = (char*)lst + lst->tag.size - size;
        init_busy(ptr_result, size);

        lst->tag.size -= size;
        Tag* tag_free = (Tag*)((char*)ptr_result - tag_size);
        tag_free->init(lst->tag.size, true);
    }

    return (char*)ptr_result + tag_size;
}

// Функция аллокации
void* myalloc(size_t size)
{
    size = size + 2 * tag_size;
    Lst* next = head;
    while (next != nullptr) {
        if (next->tag.size >= size) {
            return _myalloc(next, size);
        }
        next = next->next;
    }
    return nullptr;
}

void join(Lst* left, void* right) {
    left->tag.size += ((Tag*)right)->size;
    ((Tag*)((char*)left + left->tag.size - tag_size))->size = left->tag.size;
    ((Tag*)((char*)left + left->tag.size - tag_size))->free_flag = true;
}
// Функция освобождения
void myfree(void* p)
{
    p = (char*)p - tag_size;
    // left
    Tag* left_tag = (Tag*)((char*)p - tag_size);
    if ((void*)left_tag >= g_buf && left_tag->free_flag) {
        Lst* lst = (Lst*)((char*)p - left_tag->size);
        join(lst, p);
        p = (void*)lst;
    }
    else {
        init_free(p, head, nullptr, ((Tag*)p)->size);

        if (head != nullptr) {
            head->prev = (Lst*)p;
            ((Lst*)p)->next = head;
        }
        head = (Lst*)p;
    }

    // right
    Tag* right_tag = (Tag*)((char*)p + ((Tag*)p)->size);
    if ((void*)right_tag < (char*)g_buf + g_size && right_tag->free_flag) {
        Lst* lst = (Lst*)right_tag;
        remove_lst(lst);
        join((Lst*)p, lst);
    }
}
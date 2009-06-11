/*
 *   test the pool 
 */

#include <stdlib.h>
#include <stdio.h>

#include "vmpool.h"
#include "t3test.h"

static void test_pool(CVmPoolInMem *pool)
{
    /* translate some addresses */
    pool->get_ptr(0);
    pool->get_ptr(100);
    pool->get_ptr(1024);
    pool->get_ptr(4096);
    pool->get_ptr(4097);
    pool->get_ptr(1024);
    pool->get_ptr(0);
    pool->get_ptr(10000);
    pool->get_ptr(1);
    pool->get_ptr(10001);
    pool->get_ptr(2);
    pool->get_ptr(10002);
    pool->get_ptr(4096);
    pool->get_ptr(2048);
    pool->get_ptr(4097);
    pool->get_ptr(3);

    /* done with the pool - delete it */
    delete pool;
}

/*
 *   backing store - simple implementation that just loads from a file 
 */
class CMyBS: public CVmPoolBackingStore
{
public:
    CMyBS(FILE *fp) { fp_ = fp; }
    ~CMyBS() { fclose(fp_); }

    size_t vmpbs_get_page_count() { return 50; }
    size_t vmpbs_get_common_page_size() { return 8192; }
    size_t vmpbs_get_page_size(pool_ofs_t, size_t page_size)
    {
        return page_size;
    }
    void vmpbs_load_page(pool_ofs_t ofs, size_t page_size, size_t load_size,
                         char *mem)
    {
        fseek(fp_, ofs, SEEK_SET);
        fread(mem, page_size, 1, fp_);
    }
    const char *vmpbs_alloc_and_load_page(pool_ofs_t ofs, size_t page_size,
                                          size_t load_size)
    {
        char *mem;

        mem = (char *)t3malloc(load_size);
        vmpbs_load_page(ofs, page_size, load_size, mem);
        return mem;
    }
    void vmpbs_free_page(const char *mem, pool_ofs_t, size_t)
    {
        t3free((void *)mem);
    }

private:
    FILE *fp_;
};

int main()
{
    CVmPoolInMem *pool;
    CMyBS *bs;

    /* initialize for testing */
    test_init();

    /* 
     *   create a backing store object - use an arbitrary (but fairly large)
     *   file as the data source 
     */
    bs = new CMyBS(fopen("vmregex.cpp", "r"));

    /* create an in-memory pool */
    pool = new CVmPoolInMem();

    /* initialize it with the backing store */
    pool->attach_backing_store(bs);

    /* run the tests */
    test_pool(pool);
    
#if 0 // the swapping pool has been deprecated
    /* create a swapping pool */
    pool = new CVmPoolSwap(2);

    /* initialize it */
    pool->attach_backing_store(bs);

    /* test it */
    test_pool(pool);
#endif

    /* done */
    return 0;
}


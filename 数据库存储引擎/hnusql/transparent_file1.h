#include <sys/types.h>
#include <sys/stat.h>
#include <my_dir.h>

extern PSI_memory_key hnusql_key_memory_Transparent_file1;

class Transparent_file1{
    File trans_file;//data file
    uchar* buff;//data buffer
    my_off_t lower_bound;//beginning position of buffer
    my_off_t upper_bound;//ending position of buffer
    uint buff_size;//size of buffer
public:
    Transparent_file1();
    ~Transparent_file1();
    void init_buff(File target_file);//init buffer
    uchar* ptr();
    my_off_t start();
    my_off_t end();
    char get_value(my_off_t offset);//get value in offset
    my_off_t read_next();//read next area
};

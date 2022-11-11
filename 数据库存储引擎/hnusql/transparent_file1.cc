#include <mysql/psi/mysql_file.h>
#include "transparent_file1.h"
#include "my_sys.h"

PSI_memory_key hnusql_key_memory_Transparent_file1;

//Init the buffer.We assume that we read one block for each time.
Transparent_file1::Transparent_file1() : lower_bound(0),buff_size(IO_SIZE){
    buff = (uchar*)my_malloc(hnusql_key_memory_Transparent_file1,buff_size*sizeof(uchar),MYF(MY_WME));
}
Transparent_file1::~Transparent_file1(){
    my_free(buff);
}
//Init the target file and read one block from file
void Transparent_file1::init_buff(File target_file){
    trans_file = target_file;
    lower_bound = 0;
    //seek the file before reading from it
    mysql_file_seek(trans_file,0,MY_SEEK_SET,MYF(0));
    if(trans_file && buff){
        upper_bound = mysql_file_read(trans_file,buff,buff_size,MYF(0));
    }
}
uchar* Transparent_file1::ptr(){
    return buff;
}
my_off_t Transparent_file1::start(){
    return lower_bound;
}
my_off_t Transparent_file1::end(){
    return upper_bound;
}
char Transparent_file1::get_value(my_off_t offset){
    //check boundary
    if(lower_bound <= offset && offset < upper_bound){
        return buff[offset - lower_bound];
    }
    /*
        if needed data overflows,we read the needed position
    */
    size_t bytes;
    mysql_file_seek(trans_file, offset, MY_SEEK_SET, MYF(0));
    if((bytes = mysql_file_read(trans_file,buff,buff_size,MYF(0))) == MY_FILE_ERROR){
       return 0;
    }

   lower_bound = offset;
   upper_bound = lower_bound + bytes;
   if (upper_bound == (my_off_t) offset)
   	return 0;
   return buff[0];
}
//read next block
my_off_t Transparent_file1::read_next(){
    size_t bytes;
    /*
        We don't need to seek since only the Transparent_file class
        need to access the disk and the file is stored in continuous
        address
    */
   if((bytes = mysql_file_read(trans_file,buff,buff_size,MYF(0))) == MY_FILE_ERROR){
       return (my_off_t) -1;
   }
   //end of file
   if(bytes == 0){
       return (my_off_t)-1;
   }
   lower_bound = upper_bound;
   upper_bound += bytes;
   return lower_bound;
}

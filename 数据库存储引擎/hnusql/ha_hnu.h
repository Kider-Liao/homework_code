
#include <sys/types.h>
#include <sys/stat.h>
#include <my_dir.h>
#include "transparent_file1.h"

#define DEFAULT_CHAIN_LENGTH 512

#define HNU_VERSION 1


typedef struct st_hnu_share {
    char* table_name;//the share struct use on the table
    char data_file_name[512];
    uint table_name_length;
    uint use_count;//number of threads use the share at the same time
    //make log tables work
    my_bool is_log_table;
    my_off_t data_file_length;
    //mutex used to lock the share
    mysql_mutex_t mutex;
    //read-write lock
    THR_LOCK lock;
    //check if the update file is opened
    bool update_file_opened;
    //check if the data file is opend to be wrote
    bool hnu_write_opened;
    File meta_file; //meta file des
    File hnu_write_des;
    bool crashed; //file is crashed
    ha_rows rows_recorded; //number of records in the table
    uint data_file_version; //version of the data file.The usage will be explained later
} HNU_SHARE;

//chain struct,used to record the deleted position
struct hnu_chain {
    my_off_t begin;
    my_off_t end;
};

class ha_hnu: public handler{
    THR_LOCK_DATA lock; //MYSQL lock
    HNU_SHARE *share; //share of the table
    //pointer to the file
    my_off_t current_position;
    my_off_t next_position;
    my_off_t local_data_file_length;
    my_off_t temp_file_length;
    uchar byte_buffer[IO_SIZE];//initial buffer used to initialize the buffer
    Transparent_file1* file_buff;
    File data_file;
    File update_temp_file;
    String buffer;//String used to store the value of records
    
    //Pre-allocated buffer for chain
    hnu_chain chain_buffer[DEFAULT_CHAIN_LENGTH];
    hnu_chain* chain;//first chain of the chain_list
    hnu_chain* chain_ptr;//point to end of the chain_list
    uchar chain_alloced;//determine if the pre-allocated buffer is allocated
    uint32 chain_size;

    uint local_data_file_version;
    bool records_is_known;
    MEM_ROOT blobroot; //use for the blob type

    //the function is used to find the next position to write
    bool get_write_pos(my_off_t* end_pos, hnu_chain* next_chain);
    //we don't open temp file twice
    int open_update_temp_file_if_needed();
    //init the writer and data file,we'll explain them in detailed later
    int init_hnu_writer();
    int init_data_file();

public:
    ha_hnu(handlerton* hton, TABLE_SHARE *table_arg);
    ~ha_hnu(){
        if (chain_alloced){
            my_free(chain);
        }
        if (file_buff){
            delete file_buff;
        }
        free_root(&blobroot,MYF(0));
    }
    const char* table_type() const {return "HNUSQL";}
    const char* index_type(uint inx) {return "NONE";}
    const char** bas_ext() const;
    ulonglong table_flags() const{
        return (HA_NO_TRANSACTIONS | HA_REC_NOT_IN_SEQ | HA_NO_AUTO_INCREMENT |
            HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE |
            HA_CAN_REPAIR);
    }
    ulong index_flags(uint idx, uint part, bool all_parts) const
    {
        //we don't use indexes
        return 0;
    }
    uint max_record_length() const { return HA_MAX_REC_LENGTH; }
    uint max_keys()          const { return 0; }
    uint max_key_parts()     const { return 0; }
    uint max_key_length()    const { return 0; }
    virtual double scan_time() { return (double) (stats.records+stats.deleted) / 20.0+10; }
    virtual bool fast_key_read() { return 1;}
    ha_rows estimate_rows_upper_bound() { return HA_POS_ERROR;}
    //open the file which stores the table
    int open(const char* name, int mode, uint open_options);
    //close the file
    int close(void);
    //insert a row into table
    int write_row(uchar * buf);
    //update a row during scanning
    int update_row(const uchar * old_data, uchar * new_data);
    //delete a row during scanning
    int delete_row(const uchar * buf);
    //init the parameter related to scanning
    int rnd_init(bool scan=1);
    //fetch the next record
    int rnd_next(uchar *buf);
    //get a record according to the position
    int rnd_pos(uchar * buf, uchar *pos);
    //check if it's neccessary to repair the file
    bool check_and_repair(THD *thd);
    int check(THD* thd, HA_CHECK_OPT* check_opt);
    bool is_crashed() const;
    int repair(THD* thd, HA_CHECK_OPT* check_opt);
    //do the final action after fetching all the records
    int rnd_end();
    bool auto_repair() const { return 1; }
    //store the local record
    void position(const uchar* record);
    int info(uint);
    int extra(enum ha_extra_function operation);
    //delete rows
    int delete_all_rows(void);
    //create data file
    int create(const char* name, TABLE* form, HA_CREATE_INFO* create_info);
    bool check_if_incompatible_data(HA_CREATE_INFO *info,
                                  uint table_changes);
    //summit the read-write lock
    THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
    enum thr_lock_type lock_type);

    //get or update the file length
    void get_status();
    void update_status();

    //change the format of the record to store it in file
    int change_format(uchar* buf);
    //find the current record pointed by the present pointer
    int line_to_record(uchar* buf);
    //make the chain point to the record which is not required
    int chain_append();
    my_off_t find_eoln_buff(Transparent_file1 *data_buff, my_off_t begin,my_off_t end, int *eoln_len);
};

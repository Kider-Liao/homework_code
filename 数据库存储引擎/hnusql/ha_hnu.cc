#include "my_global.h"
#include "sql_class.h"                          
#include <mysql/plugin.h>
#include <mysql/psi/mysql_file.h>
#include "ha_hnu.h"
#include "probes_mysql.h"

#include <algorithm>

using std::min;
using std::max;

#define META_BUFFER_SIZE sizeof(uchar) + sizeof(uchar) + sizeof(ulonglong) \
        + sizeof(uchar)
#define HNU_CHECK_HEADER 254
#define BLOB_MEMROOT_ALLOC_SIZE 8192

//file extension used in the storage engine
#define HDATA_EXT ".HDATA"
#define HMETA_EXT ".HMETA"
#define HTEMP_EXT ".HTEMP"

#define FIELD_SEPERATOR ' '

static HNU_SHARE *get_share(const char *table_name, TABLE *table);
static int free_share(HNU_SHARE *share);
static int read_meta_file(File meta_file, ha_rows *rows);
static int write_meta_file(File meta_file, ha_rows rows, bool dirty);

extern "C" void hnu_get_status(void* param, int concurrent_insert);
extern "C" void hnu_update_status(void* param);
extern "C" my_bool hnu_check_status(void* param);

//global mutex used to control the allocation of hash
mysql_mutex_t hnu_mutex;
//hash table used to store the share structure
static HASH hnu_hash_table;
static handler* hnu_create_handler(handlerton *hton,
                                    TABLE_SHARE *table, 
                                    MEM_ROOT *mem_root);

/* function definition in the storage engine*/

//funtion used for sorting chains with qsort()
int sort_set (hnu_chain* a, hnu_chain* b){
    //we just need to compare the beginning of chain since the record won't stagger
    return ( a->begin > b->begin ? 1 : ( a->begin < b->begin ? -1 : 0 ) );
}

//get the hash key of hash table
static uchar* hnu_get_key(HNU_SHARE* share, size_t* length,
                          my_bool not_used MY_ATTRIBUTE((unused))){
    *length = share->table_name_length;
    return (uchar*) share->table_name;                          
}

static PSI_memory_key hnusql_key_memory_hnu_share;
static PSI_memory_key hnusql_key_memory_blobroot;
static PSI_memory_key hnusql_key_memory_hnu_set;
static PSI_memory_key hnusql_key_memory_row;

#ifdef HAVE_PSI_INTERFACE

static PSI_mutex_key hnusql_key_mutex_hnu, hnusql_key_mutex_HNU_SHARE_mutex;

static PSI_mutex_info all_hnu_mutexes[]=
{
  { &hnusql_key_mutex_hnu, "hnu", PSI_FLAG_GLOBAL},
  { &hnusql_key_mutex_HNU_SHARE_mutex, "HNU_SHARE::mutex", 0}
};

static PSI_file_key hnusql_key_file_metadata, hnusql_key_file_data,
  hnusql_key_file_update;

static PSI_file_info all_hnu_files[]=
{
  { &hnusql_key_file_metadata, "metadata", 0},
  { &hnusql_key_file_data, "data", 0},
  { &hnusql_key_file_update, "update", 0}
};

static PSI_memory_info all_hnu_memory[]=
{
  { &hnusql_key_memory_hnu_share, "HNU_SHARE", PSI_FLAG_GLOBAL},
  { &hnusql_key_memory_blobroot, "blobroot", 0},
  { &hnusql_key_memory_hnu_set, "hnu_set", 0},
  { &hnusql_key_memory_row, "row", 0},
  { &hnusql_key_memory_Transparent_file1, "Transparent_file1", 0}
};

static void init_hnu_psi_keys(void)
{
  const char* category= "hnusql";
  int count;

  count= array_elements(all_hnu_mutexes);
  mysql_mutex_register(category, all_hnu_mutexes, count);

  count= array_elements(all_hnu_files);
  mysql_file_register(category, all_hnu_files, count);

  count= array_elements(all_hnu_memory);
  mysql_memory_register(category, all_hnu_memory, count);
}
#endif /* HAVE_PSI_INTERFACE */

/* initialize the handlerton for the thread to get ready for 
subsequent action*/

//use the constructor of handler and init the parameters of
//hnu class
ha_hnu::ha_hnu(handlerton *hton, TABLE_SHARE *table_arg)
:handler(hton,table_arg),current_position(0),next_position(0),
local_data_file_length(0),temp_file_length(0),chain_alloced(0),
chain_size(DEFAULT_CHAIN_LENGTH),local_data_file_version(0),
records_is_known(0){
    //pre-alloc an initial place for the buffer
    buffer.set((char*)byte_buffer, IO_SIZE, &my_charset_bin);
    chain = chain_buffer;
    file_buff = new Transparent_file1();
    init_alloc_root(hnusql_key_memory_blobroot,
                  &blobroot, BLOB_MEMROOT_ALLOC_SIZE, 0);
}

//create a handler for the thread and init the parameters
static handler* hnu_create_handler(handlerton *hton,
                                    TABLE_SHARE *table, 
                                    MEM_ROOT *mem_root){
    return new (mem_root) ha_hnu(hton, table);
}

//init the handlerton.just do some simple initial operation 
static int hnu_init_func(void* p){
    handlerton* hnu_hton;

    #ifdef HAVE_PSI_INTERFACE
        init_hnu_psi_keys();
    #endif

    hnu_hton = (handlerton*)p;
    //init the global mutex.
    mysql_mutex_init(hnusql_key_mutex_hnu, &hnu_mutex, MY_MUTEX_INIT_FAST);
    (void) my_hash_init(&hnu_hash_table,system_charset_info,32,0,0,
                      (my_hash_get_key) hnu_get_key,0,0,
                      hnusql_key_memory_hnu_share);
    hnu_hton->state = SHOW_OPTION_YES;
    hnu_hton->db_type = DB_TYPE_HNUSQL_DB;
    hnu_hton->create = hnu_create_handler;
    hnu_hton->flags= (HTON_CAN_RECREATE | HTON_SUPPORT_LOG_TABLES | 
                     HTON_NO_PARTITION);
    return 0;
}

static int hnu_done_func(void *p)
{
  my_hash_free(&hnu_hash_table);
  mysql_mutex_destroy(&hnu_mutex);

  return 0;
}

/* define the extensions for the handler*/
static const char *ha_hnu_exts[] = {
  HDATA_EXT,
  HMETA_EXT,
  NullS
};

const char **ha_hnu::bas_ext() const
{
  return ha_hnu_exts;
}

/* check and modify the status*/
void hnu_get_status(void* param, int concurrent_insert){
    ha_hnu* hnu = (ha_hnu*) param;
    hnu->get_status();
}

void hnu_update_status(void* param){
    ha_hnu* hnu = (ha_hnu*)param;
    hnu->update_status();
}

my_bool hnu_check_status(void* param){
    return 0;
}

void ha_hnu::get_status(){
    if(share->is_log_table){
        mysql_mutex_lock(&share->mutex);
        local_data_file_length= share->data_file_length;
        mysql_mutex_unlock(&share->mutex);
        return;
    }
    local_data_file_length= share->data_file_length;
}

/* correct state of the table */
void ha_hnu::update_status(){
    share->data_file_length = local_data_file_length;
}

/* write the metadata of data file.we ignore some parameters */
static int write_meta_file(File meta_file, ha_rows rows, bool dirty){
    uchar meta_buffer[META_BUFFER_SIZE];
    uchar* ptr = meta_buffer;
    DBUG_ENTER("ha_hnu::write_meta_file");

    *ptr = (uchar)HNU_CHECK_HEADER;
    ptr+= sizeof(uchar);
    *ptr = (char)HNU_VERSION;
    ptr+= sizeof(uchar);
    int8store(ptr, (ulonglong)rows);
    ptr+= sizeof(ulonglong);
    *ptr = (uchar)dirty;

    //seek the position of the file
    mysql_file_seek(meta_file, 0, MY_SEEK_SET, MYF(0));
    if (mysql_file_write(meta_file,(uchar*)meta_buffer,META_BUFFER_SIZE,0)!=META_BUFFER_SIZE){
        DBUG_RETURN(-1);
    }
    //submit the write
    mysql_file_sync(meta_file, MYF(MY_WME));

    DBUG_RETURN(0);
}

/* read the metadata of data file.get number of records from metadata*/
static int read_meta_file(File meta_file, ha_rows* rows){
    uchar meta_buffer[META_BUFFER_SIZE];
    uchar* ptr = meta_buffer;
    DBUG_ENTER("ha_hnu::read_meta_file");

    mysql_file_seek(meta_file, 0, MY_SEEK_SET, MYF(0));
    if (mysql_file_read(meta_file, (uchar*)meta_buffer, META_BUFFER_SIZE, 0)
      != META_BUFFER_SIZE)
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

    ptr+= sizeof(uchar)*2;
    //read records number
    *rows = (ha_rows)uint8korr(ptr);
    ptr+=sizeof(ulonglong);
    //check if the meta file is crashed
    if ((meta_buffer[0] != (uchar)HNU_CHECK_HEADER) ||
      ((bool)(*ptr)== TRUE))
    DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

    mysql_file_sync(meta_file, MYF(MY_WME));

    DBUG_RETURN(0);
}

int ha_hnu::init_hnu_writer(){
  DBUG_ENTER("ha_hnu::init_hnu_writer");

  (void)write_meta_file(share->meta_file, share->rows_recorded, TRUE);

  if((share->hnu_write_des = 
        mysql_file_open(hnusql_key_file_data,
                        share->data_file_name, O_RDWR|O_APPEND,
                        MYF(MY_WME))) == -1){
    DBUG_PRINT("info", ("Could not open tina file writes"));
    share->crashed = TRUE;
    DBUG_RETURN(my_errno()? my_errno(): -1);
  }
  share->hnu_write_opened = TRUE;

  DBUG_RETURN(0);
}

bool ha_hnu::is_crashed() const
{
  DBUG_ENTER("ha_hnu::is_crashed");
  DBUG_RETURN(share->crashed);
}

/* create the data file for tables */
int ha_hnu::create(const char* name, TABLE* table_arg,
                    HA_CREATE_INFO *create_info){
    //use to accept the temp file_name
    char file_name[FN_REFLEN];
    File create_file;
    DBUG_ENTER("ha_hnu::create");

    //We don't store NULL value.So we should check the field
    for (Field **field= table_arg->s->field; *field; field++)
    {
        if ((*field)->real_maybe_null())
        {
            my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "nullable columns");
            DBUG_RETURN(HA_ERR_UNSUPPORTED);
        }
    }

    //create a meta file.fn_format() function is used to format the file name 
    if((create_file = mysql_file_create(hnusql_key_file_metadata,
                                      fn_format(file_name, name, "", HMETA_EXT,
                                                MY_REPLACE_EXT|MY_UNPACK_FILENAME),
                                      0, O_RDWR | O_TRUNC, MYF(MY_WME))) < 0){
        DBUG_RETURN(-1);
    }
    //write meta file to initialize the parameters of data file
    write_meta_file(create_file, 0, FALSE);
    mysql_file_close(create_file, MYF(0));

    //create a data file 
    if((create_file = mysql_file_create(hnusql_key_file_data,
                                      fn_format(file_name, name, "", HDATA_EXT,
                                                MY_REPLACE_EXT|MY_UNPACK_FILENAME),
                                      0, O_RDWR | O_TRUNC, MYF(MY_WME))) < 0){
        DBUG_RETURN(-1);
    }

    mysql_file_close(create_file, MYF(0));

    DBUG_RETURN(0);
}

/* alloc the share struture of the table and init the parameters */
static HNU_SHARE* get_share(const char* table_name, TABLE* TABLE){
    HNU_SHARE* share;
    char meta_file_name[FN_REFLEN];
    MY_STAT file_stat;
    char* tmp_name;
    uint length;

    //only one thread can access here to get share, or it may cause problems
    mysql_mutex_lock(&hnu_mutex);
    length = (uint)strlen(table_name);

    /* if share is not in the hash, create a new share and initialize its members */
    if(!(share = (HNU_SHARE*) my_hash_search(&hnu_hash_table,(uchar*)table_name,length))){
        /* if we fail to malloc space for share, then return */
        if(!my_multi_malloc(hnusql_key_memory_hnu_share, MYF(MY_WME | MY_ZEROFILL),
                            &share, sizeof(*share), &tmp_name, length+1, NullS)){
            mysql_mutex_unlock(&hnu_mutex);
            return NULL;
        }

        /* initialize share members */
        share->use_count= 0;
        share->is_log_table= FALSE;
        share->table_name_length= length;
        /* use the tmp_name so that it won't be free because of table_name */
        share->table_name= tmp_name;
        share->crashed= FALSE;
        share->rows_recorded= 0;
        share->update_file_opened= FALSE;
        share->hnu_write_opened= FALSE;
        share->data_file_version= 0;
        my_stpcpy(share->table_name, table_name);
        fn_format(share->data_file_name, table_name, "", HDATA_EXT, MY_REPLACE_EXT|MY_UNPACK_FILENAME);
        fn_format(meta_file_name, table_name, "", HMETA_EXT, MY_REPLACE_EXT|MY_UNPACK_FILENAME);

        /* get the file state */
        if(mysql_file_stat(hnusql_key_file_data, share->data_file_name, &file_stat, MYF(MY_WME)) == NULL){
            goto error;
        }
        share->data_file_length = file_stat.st_size;

        /* insert the share into hash table */
        if (my_hash_insert(&hnu_hash_table, (uchar*) share))
            goto error;
        thr_lock_init(&share->lock);
        mysql_mutex_init(hnusql_key_mutex_HNU_SHARE_mutex, &share->mutex, MY_MUTEX_INIT_FAST);
        /* read records from meta_file */
        if(((share->meta_file = mysql_file_open(hnusql_key_file_metadata, meta_file_name,
                                                O_RDWR|O_CREAT,MYF(MY_WME))) == -1) || 
                                                read_meta_file(share->meta_file, &share->rows_recorded)){
            share->crashed = true;
        }
    }

    share->use_count++;
    mysql_mutex_unlock(&hnu_mutex);

    return share;

error: 
    mysql_mutex_unlock(&hnu_mutex);
    my_free(share);

    return NULL;
}

/* free table share after using the table -- free the lock controls*/
static int free_share(HNU_SHARE* share){
    DBUG_ENTER("ha_hnu::free_share");
    mysql_mutex_lock(&hnu_mutex);
    int result = 0;
    //if no user is using the share
    if(!--share->use_count){
        (void)write_meta_file(share->meta_file, share->rows_recorded, share->crashed ? TRUE :FALSE);
        if(mysql_file_close(share->meta_file, MYF(0))){
            result = 1;
        }
        if(share->hnu_write_opened){
            if(mysql_file_close(share->hnu_write_des, MYF(0))){
                result = 1;
            }
            share->hnu_write_opened = FALSE;
        }
        //delete the hash
        my_hash_delete(&hnu_hash_table, (uchar*) share);
        thr_lock_delete(&share->lock);
        mysql_mutex_destroy(&share->mutex);
        my_free(share);
    }
    mysql_mutex_unlock(&hnu_mutex);
    DBUG_RETURN(result);
}

/* open the file of the table and get the share of the table */
int ha_hnu::open(const char *name, int mode, uint open_options){
    DBUG_ENTER("ha_hnu::open");

    //get the share and check if it's out of memory
    if(!(share = get_share(name, table))){
        DBUG_RETURN(HA_ERR_OUT_OF_MEM);
    }

    /*if meta file is crashed and we don't open to repair it, then just return */
    if(share->crashed && !(open_options & HA_OPEN_FOR_REPAIR)){
        free_share(share);
        DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
    }

    local_data_file_version = share->data_file_version;
    /*if fail to open the file then release the share and return */
    if((data_file = mysql_file_open(hnusql_key_file_data,share->data_file_name,
                                    O_RDONLY,MYF(MY_WME))) == -1){
        free_share(share);
        DBUG_RETURN(my_errno() ? my_errno() : -1);
    }

    /* init the thread lock */
    thr_lock_data_init(&share->lock, &lock, (void*) this);
    ref_length= sizeof(my_off_t);
    /* use to update the  status */
    share->lock.get_status= hnu_get_status;
    share->lock.update_status= hnu_update_status;
    share->lock.check_status= hnu_check_status;

    DBUG_RETURN(0);
}

/* close the file and release the share*/
int ha_hnu::close(void)
{
  int rc= 0;
  DBUG_ENTER("ha_hnu::close");
  rc= mysql_file_close(data_file, MYF(0));
  DBUG_RETURN(free_share(share) || rc);
}

/*
  All table scans call this first.
  The order of a table scan is:

  ha_hnu::store_lock
  ha_hnu::external_lock
  ha_hnu::info
  ha_hnu::rnd_init
  ha_hnu::extra
  ENUM HA_EXTRA_CACHE   Cash record in HA_rrnd()
  ha_hnu::rnd_next
  ha_hnu::rnd_next
  ha_hnu::rnd_next
  ha_hnu::rnd_next
  ha_hnu::rnd_next
  ha_hnu::rnd_next
  ha_hnu::rnd_next
  ha_hnu::rnd_next
  ha_hnu::rnd_next
  ha_hnu::extra
  ENUM HA_EXTRA_NO_CACHE   End cacheing of records (def)
  ha_hnu::external_lock
  ha_hnu::extra
  ENUM HA_EXTRA_RESET   Reset database to after open

  Each call to ::rnd_next() represents a row returned in the can. When no more
  rows can be returned, rnd_next() returns a value of HA_ERR_END_OF_FILE.
  The ::info() call is just for the optimizer.

*/

int ha_hnu::init_data_file(){
    /*it's neccessary to check the file version since if one thread changes the
    file after one thread opens the file and waits for the lock, it may cause some
    errors*/
    if(local_data_file_version != share->data_file_version){
        local_data_file_version = share->data_file_version;
        if (mysql_file_close(data_file, MYF(0)) ||
        (data_file= mysql_file_open(hnusql_key_file_data,
                                    share->data_file_name, O_RDONLY,
                                    MYF(MY_WME))) == -1)
        return my_errno() ? my_errno() : -1;
    }
    file_buff->init_buff(data_file);
    return 0;
}

/* submit the read-write lock */
THR_LOCK_DATA **ha_hnu::store_lock(THD *thd,
                                    THR_LOCK_DATA **to,
                                    enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
    lock.type=lock_type;
  *to++= &lock;
  return to;
}

/*init the buffer and parameters about scan*/
int ha_hnu::rnd_init(bool scan){
    DBUG_ENTER("ha_hnu::rnd_init");
    if (share->crashed || init_data_file()){
      // reopen data file
      DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
    }
    current_position = next_position = 0;
    stats.records = 0;
    records_is_known = 0;
    chain_ptr = chain;
    DBUG_RETURN(0);
}

/* find lines from the file and return the length of ending */
my_off_t ha_hnu::find_eoln_buff(Transparent_file1 *data_buff, my_off_t begin,
                     my_off_t end, int *eoln_len)
{
  *eoln_len = 0;
  for(my_off_t x = begin; x < end; x++){
    if(data_buff->get_value(x) == '\n') // '\n'
      *eoln_len = 1;
    else{
      if(data_buff->get_value(x) == '\r'){
        if (x + 1 == end || (data_buff->get_value(x + 1) != '\n')) // '\r'
          *eoln_len= 1;
        else // '\r\n'
          *eoln_len= 2;
      }
    }
    if (*eoln_len) return x;
  }
  return 0;
}

/* find current row and store it in the field */
/* according to ha_tina::find_current_row in CSV */
int ha_hnu::line_to_record(uchar* buf){
  my_off_t end_offset, curr_offset = current_position;
  int eoln_len;
  my_bitmap_map *org_bitmap;
  int error;
  bool read_all;
  DBUG_ENTER("ha_hnu::line_to_record");

  free_root(&blobroot, MYF(0));

  // cannot find the end of a line
  if((end_offset = 
        find_eoln_buff(file_buff, current_position, 
                       local_data_file_length, &eoln_len)) == 0)
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  read_all = !bitmap_is_clear_all(table->write_set);
  org_bitmap = dbug_tmp_use_all_columns(table, table->write_set);
  error = HA_ERR_CRASHED_ON_USAGE;

  memset(buf, 0, table->s->null_bytes);

  for (Field **field = table->field; *field; field++){
    char curr_char;
    buffer.length(0);
    if(curr_offset >= end_offset) goto err;
    curr_char = file_buff->get_value(curr_offset);

    // first character is a quote ", meaning the value is a string
    if(curr_char == '"'){

      /* jump past the quote */
      curr_offset++;

      // loop until find the ending " to get the value of the current field
      for(; curr_offset < end_offset; curr_offset++){
        curr_char = file_buff->get_value(curr_offset);

        // find the ending "
        /* Note: each field in a line is seperated by space' ' not comma',' */
        if(curr_char == '"' && 
            (curr_offset == end_offset - 1 || 
             file_buff->get_value(curr_offset + 1) == FIELD_SEPERATOR)){
          curr_offset += 2;
          break;
        }

        // find the escaped character '\'
        if(curr_char == '\\' && curr_offset != end_offset-1){
          curr_char = file_buff->get_value(++curr_offset);
          if(curr_char == 'r') buffer.append('\r');
          else if(curr_char == 'n') buffer.append('\n');
          else if(curr_char == '\\' || curr_char == '"') 
            buffer.append(curr_char);
          else{ // escaped character is in the value input by user
            buffer.append('\\');
            buffer.append(curr_char);
          }
        }
        // ordinary symbol
        else{
          if (curr_offset == end_offset-1) goto err;
          buffer.append(curr_char);
        }
      }
    }

    else{
      for(; curr_offset < end_offset; curr_offset++){
        curr_char = file_buff->get_value(curr_offset);

        // move pass the seperator ' '
        if(curr_char == FIELD_SEPERATOR){
          curr_offset++;
          break;
        }
        
        // find the escaped character '\'
        if(curr_offset == '\\' && curr_offset != end_offset-1){
          curr_char = file_buff->get_value(++curr_offset);
          if(curr_char == 'r') buffer.append('\r');
          else if(curr_char == 'n') buffer.append('\n');
          else if(curr_char == '\\' || curr_char == '"') 
            buffer.append(curr_char);
          else{ // escaped character is in the value input by user
            buffer.append('\\');
            buffer.append(curr_char);
          }
        }

        else{ 
          if(curr_offset == end_offset-1 && curr_char == '"') goto err;
          buffer.append(curr_char);
        }
      }
    }

    if(read_all || bitmap_is_set(table->read_set, (*field)->field_index)){
      bool is_enum = ((*field)->real_type() == MYSQL_TYPE_ENUM);
      
      if ((*field)->store(buffer.ptr(), buffer.length(), buffer.charset(),
                          is_enum? CHECK_FIELD_IGNORE: CHECK_FIELD_WARN)){
        if(!is_enum) goto err;
      }

      if((*field)->flags & BLOB_FLAG){
        Field_blob *blob = *(Field_blob**)field;
        uchar *src, *tgt;
        uint length, packlength;

        packlength = blob->pack_length_no_ptr();
        length = blob->get_length(blob->ptr);
        memcpy(&src, blob->ptr + packlength, sizeof(char*));
        if(src){
          tgt = (uchar*) alloc_root(&blobroot, length);
          memmove(tgt, src, length);
          memcpy(blob->ptr + packlength, tgt, sizeof(char*));
        }
      }
    }
  }

  next_position = end_offset + eoln_len;
  error = 0;

err:
  dbug_tmp_restore_column_map(table->write_set, org_bitmap);
  DBUG_RETURN(error);
}

/* rnd_next is used to fetch one record from the file */
int ha_hnu::rnd_next(uchar* buf){
    int result;
    DBUG_ENTER("ha_hnu::rnd_next");
    MYSQL_READ_ROW_START(table_share->db.str, table_share->table_name.str,
                       TRUE);
    
    if(share->crashed){
      result = HA_ERR_CRASHED_ON_USAGE;
      goto end;
    }

    ha_statistic_increment(&SSV::ha_read_rnd_next_count);
    current_position = next_position;

    // empty file
    if(!local_data_file_length){
      result = HA_ERR_END_OF_FILE;
      goto end;
    }

    if((result = line_to_record(buf)))
      goto end;
    
    stats.records++;
    result = 0;
end:
  MYSQL_READ_ROW_DONE(result);
  DBUG_RETURN(result);
}

/*add the empty space into chain*/
int ha_hnu::chain_append(){
  if(chain_ptr != chain && (chain_ptr-1)->end == current_position)
    (chain_ptr-1)->end = next_position;

  else{
    // set up for next position
    if((size_t)(chain_ptr - chain) == (chain_size-1)){
      my_off_t location = chain_ptr - chain;
      chain_size += DEFAULT_CHAIN_LENGTH;
      if(chain_alloced){
        if((chain = (hnu_chain*) my_realloc(hnusql_key_memory_hnu_set,
                                            (uchar*)chain,
                                            chain_size*sizeof(hnu_chain), MYF(MY_WME))) == NULL)
          return -1;
      }
      else{
        hnu_chain *ptr = (hnu_chain *) my_malloc(hnusql_key_memory_hnu_set,
                                                 chain_size * sizeof(hnu_chain), 
                                                 MYF(MY_WME));
        memcpy(ptr, chain, DEFAULT_CHAIN_LENGTH * sizeof(hnu_chain));
        chain = ptr;
        chain_alloced++;
      }
      chain_ptr = chain + location;
    }
    chain_ptr->begin = current_position;
    chain_ptr->end = next_position;
    chain_ptr++;
  }

  return 0;
}

/* change the format of the buffer to better store it */
// according to ha_tina::encode_quote in CSV
int ha_hnu::change_format(uchar* buf){
  char attribute_buffer[1024];
  String attribute(attribute_buffer, sizeof(attribute_buffer), 
                   &my_charset_bin);
  
  my_bitmap_map *org_bitmap = dbug_tmp_use_all_columns(table, table->read_set);
  buffer.length(0);

  for (Field **field = table->field; *field; field++){
    const char *ptr, *end_ptr;
    const bool was_null = (*field)->is_null();

    if(was_null){
      (*field)->set_default();
      (*field)->set_notnull();
    }

    (*field)->val_str(&attribute, &attribute);

    if (was_null) (*field)->set_null();

    if((*field)->str_needs_quotes()){
      ptr = attribute.ptr();
      end_ptr = attribute.length() + ptr;

      buffer.append('"');

      for(; ptr < end_ptr; ptr++) {
        if (*ptr == '"'){
          buffer.append('\\');
          buffer.append('"');
        }
        else if (*ptr == '\r'){
          buffer.append('\\');
          buffer.append('r');
        }
        else if (*ptr == '\\'){
          buffer.append('\\');
          buffer.append('\\');
        }
        else if (*ptr == '\n'){
          buffer.append('\\');
          buffer.append('n');
        }
        else
          buffer.append(*ptr);
      }

      buffer.append('"');
    }

    else{
      buffer.append(attribute);
    }

    buffer.append(FIELD_SEPERATOR);
  }
  // remove the last seperator, add a line feed
  buffer.length(buffer.length() - 1);
  buffer.append('\n');

  dbug_tmp_restore_column_map(table->read_set, org_bitmap);
  return (buffer.length());
}

/* insert a record to file*/
int ha_hnu::write_row(uchar* buf){
    int size;
    DBUG_ENTER("ha_hnu::write_row");

    if(share->crashed) DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);

    ha_statistic_increment(&SSV::ha_write_count);

    size = change_format(buf);

    if(!share->hnu_write_opened){
      if(init_hnu_writer()) DBUG_RETURN(-1);
    }

    if(mysql_file_write(share->hnu_write_des, (uchar*)buffer.ptr(), size,
                        MYF(MY_WME | MY_NABP)))
      DBUG_RETURN(-1);
    
    local_data_file_length += size;

    mysql_mutex_lock(&share->mutex);
    share->rows_recorded++;
    if(share->is_log_table) update_status();
    mysql_mutex_unlock(&share->mutex);

    stats.records++;
    DBUG_RETURN(0);
}


/*open temp file*/
int ha_hnu::open_update_temp_file_if_needed(){
  char updated_fname[FN_REFLEN]; 

  //根据share中的update_file_opened标签来判断是否打开了临时文件
  if (share->update_file_opened == FALSE) //没有打开file 
  {
    if ((update_temp_file=
           mysql_file_create(hnusql_key_file_update,
                             fn_format(updated_fname, share->table_name,
                                       "", HTEMP_EXT,
                                       MY_REPLACE_EXT | MY_UNPACK_FILENAME),
                             0, O_RDWR | O_TRUNC, MYF(MY_WME))) < 0)
      return 1;

    //设置share中的标志位
    share->update_file_opened= TRUE;
    temp_file_length= 0; //长度是0
  }
  return 0;
}

/* update a record */
int ha_hnu::update_row(const uchar * old_data, uchar * new_data){
    int size;
    int rc = -1;
    DBUG_ENTER("ha_hnu::update_row");
    //首先对计数器进行自增
    ha_statistic_increment(&SSV::ha_update_count);
    //获取数据的长度
    size = change_format(new_data);

    //将被更新的数据项插入到chain中
    if (chain_append()) goto err;
    //打开临时文件
    if (open_update_temp_file_if_needed()) goto err;
    //将新的数据写入到临时文件中
    if (mysql_file_write(update_temp_file, (uchar*)buffer.ptr(), size, MYF(MY_WME | MY_NABP)))
      goto err;
    //更新临时文件的长度
    temp_file_length += size;

    rc = 0;
    //assert语句：确保不会对log表进行更新操作
    assert(!share->is_log_table);

err:
    DBUG_PRINT("info",("rc = %d", rc));
    DBUG_RETURN(rc);
}

/* delete a record */
int ha_hnu::delete_row(const uchar* buf){
    DBUG_ENTER("ha_hnu::delete_row");

    //计数器自增
    ha_statistic_increment(&SSV::ha_delete_count);

    //将数据插入到chain中
    if (chain_append()) DBUG_RETURN(-1);
    //记录数--
    stats.records -= 1;
    
    //update the info in share
    assert(share->rows_recorded);
    // get the lock
    mysql_mutex_lock(&share->mutex);
    //update row_recorded
    share->rows_recorded--;
    // release the lock
    mysql_mutex_unlock(&share->mutex);

    //check:DELETE should never happen on the log table
    assert(!share->is_log_table);

    DBUG_RETURN(0);
}

/* stored a row in specific position */
void ha_hnu::position(const uchar *record)
{
  DBUG_ENTER("ha_hnu::position");
  my_store_ptr(ref, ref_length, current_position);
  DBUG_VOID_RETURN;
}

/* used to fetch a row from a position */
int ha_hnu::rnd_pos(uchar * buf, uchar *pos)
{
  int rc;
  DBUG_ENTER("ha_hnu::rnd_pos");
  MYSQL_READ_ROW_START(table_share->db.str, table_share->table_name.str,
                       FALSE);
  ha_statistic_increment(&SSV::ha_read_rnd_count);
  current_position= my_get_ptr(pos,ref_length);
  rc= line_to_record(buf);
  MYSQL_READ_ROW_DONE(rc);
  DBUG_RETURN(rc);
}

int ha_hnu::info(uint flag)
{
  DBUG_ENTER("ha_hnu::info");
  /* This is a lie, but you don't want the optimizer to see zero or 1 */
  if (!records_is_known && stats.records < 2) 
    stats.records= 2;
  DBUG_RETURN(0);
}

int ha_hnu::extra(enum ha_extra_function operation)
{
  DBUG_ENTER("ha_hnu::extra");
 if (operation == HA_EXTRA_MARK_AS_LOG_TABLE)
 {
   mysql_mutex_lock(&share->mutex);
   share->is_log_table= TRUE;
   mysql_mutex_unlock(&share->mutex);
 }
  DBUG_RETURN(0);
}

/* get the next write pos to write the temp file */
//到时候再回来写这个函数
bool ha_hnu::get_write_pos(my_off_t* end_pos, hnu_chain* next_chain){
  if (next_chain == chain_ptr)  *end_pos= file_buff->end(); //end_pos is set to the upper bound of the file_buff
  
  else *end_pos= min(file_buff->end(), next_chain->begin);
  
  return (next_chain != chain_ptr) && (*end_pos == next_chain->begin);
}

/* after scan all rows, use rnd_end function to do final operation */
int ha_hnu::rnd_end(){
    char updated_fname[FN_REFLEN];
    my_off_t file_buffer_start= 0;
    DBUG_ENTER("ha_hnu::rnd_end");

    //.............
    free_root(&blobroot, MYF(0));
    records_is_known= 1;

    //check if the table need to be update/delete
    if((chain_ptr - chain)  > 0){
        hnu_chain * ptr = chain;

        //set the file_ptr to the beginning of the file
        file_buff->init_buff(data_file);
        // since the set in chain in random , we need to sort it before use it
        my_qsort(chain,(size_t)(chain_ptr - chain),sizeof(hnu_chain),(qsort_cmp)sort_set);

        my_off_t write_begin = 0;
        my_off_t write_end;

        //create temp file if it is not created
        if (open_update_temp_file_if_needed()) DBUG_RETURN(-1);
        
        //write the file 
        while (file_buffer_start != (my_off_t)-1){
          bool hole = get_write_pos(&write_end,ptr);
          // get the length
          my_off_t length = write_end - write_begin;

          //if length > 0 ,it means there is sth to write
          if(length){
            if (mysql_file_write(update_temp_file,
                      (uchar*) (file_buff->ptr() +
                                (write_begin - file_buff->start())),
                      (size_t)length, MYF_RW))
            goto error;
            temp_file_length+= length;
          }
          //it means there is a hole need to skip
          if (hole){
            //将指针逐个字节移动到要跳过的数据之后,同时要保证不会越过该盘块
            while (file_buff->end() <= ptr->end && file_buffer_start != (my_off_t)-1){
              file_buffer_start= file_buff->read_next();
            }
            //已经越过了这个hole
            write_begin = ptr->end;
            ptr++;
          }
          else{
            write_begin = write_end;
          }
          //读取下一个缓冲区的数据
          if (write_end == file_buff->end())
            file_buffer_start= file_buff->read_next(); 
        }

        //close temp file
        if (mysql_file_sync(update_temp_file, MYF(MY_WME)) ||
            mysql_file_close(update_temp_file, MYF(0)))
          DBUG_RETURN(-1);
        // }
        share->update_file_opened = FALSE;
        // deal with  hnu_write_opened
        if(share->hnu_write_opened){
          if (mysql_file_close(share->hnu_write_des, MYF(0)))
            DBUG_RETURN(-1);
            /*
              Mark that the writer fd is closed, so that init_tina_writer()
              will reopen it later.
            */
          share->hnu_write_opened= FALSE;
        }

        //关闭原先的表，并将temp的表重命名为原来的表
        if (mysql_file_close(data_file, MYF(0)) ||
            mysql_file_rename(hnusql_key_file_data,
                          fn_format(updated_fname, share->table_name,
                                    "", HTEMP_EXT,
                                    MY_REPLACE_EXT | MY_UNPACK_FILENAME),
                          share->data_file_name, MYF(0)))
          DBUG_RETURN(-1);

        // open the rename file
        if ((data_file= mysql_file_open(hnusql_key_file_data,
                                    share->data_file_name,
                                    O_RDONLY, MYF(MY_WME))) == -1)
          DBUG_RETURN(my_errno() ? my_errno() : -1);

        //update share
        share->data_file_version += 1;
        local_data_file_version= share->data_file_version;
        
        //write meta data
        (void)write_meta_file(share->meta_file, share->rows_recorded, FALSE);

        //update local length
        local_data_file_length= temp_file_length;
	}

    DBUG_RETURN(0);
error:
    mysql_file_close(update_temp_file, MYF(0));
    share->update_file_opened= FALSE;
    DBUG_RETURN(-1);
}

/* repair the table if it is crashed */
int ha_hnu::repair(THD* thd, HA_CHECK_OPT* check_opt){
    DBUG_ENTER("ha_hnu::repair");
    DBUG_RETURN(HA_ADMIN_OK);
}

/* delete all rows */
int ha_hnu::delete_all_rows()
{
  int rc;
  DBUG_ENTER("ha_hnu::delete_all_rows");

  if (!records_is_known)
  {
    set_my_errno(HA_ERR_WRONG_COMMAND);
    DBUG_RETURN(HA_ERR_WRONG_COMMAND);
  }

  if (!share->hnu_write_opened)
    if (init_hnu_writer())
      DBUG_RETURN(-1);

  /* Truncate the file to zero size */
  rc= mysql_file_chsize(share->hnu_write_des, 0, 0, MYF(MY_WME));

  stats.records=0;
  /* Update shared info */
  mysql_mutex_lock(&share->mutex);
  share->rows_recorded= 0;
  mysql_mutex_unlock(&share->mutex);
  local_data_file_length= 0;
  DBUG_RETURN(rc);
}

/* check the file */
int ha_hnu::check(THD* thd, HA_CHECK_OPT* check_opt){
    DBUG_ENTER("ha_hnu::check");
    DBUG_RETURN(HA_ADMIN_OK);
}

bool ha_hnu::check_and_repair(THD *thd)
{
  HA_CHECK_OPT check_opt;
  DBUG_ENTER("ha_hnu::check_and_repair");

  check_opt.init();

  DBUG_RETURN(repair(thd, &check_opt));
}

bool ha_hnu::check_if_incompatible_data(HA_CREATE_INFO *info,
					   uint table_changes)
{
  return COMPATIBLE_DATA_YES;
}

struct st_mysql_storage_engine hnusql_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

mysql_declare_plugin(hnusql)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &hnusql_storage_engine,
  "HNUSQL",
  "Brian Aker, MySQL AB",
  "HNUSQL storage engine",
  PLUGIN_LICENSE_GPL,
  hnu_init_func, /* Plugin Init */
  hnu_done_func, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL,                       /* config options                  */
  0,                          /* flags                           */
}
mysql_declare_plugin_end;

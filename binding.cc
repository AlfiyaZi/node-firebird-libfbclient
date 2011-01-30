#include <ibase.h>
#include <node.h>
#include <node_events.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "align.h"



using namespace v8;
using namespace node;

class Connection;

char * ErrorMessage(const ISC_STATUS *pvector, char *err_msg, int max_len){
    err_msg[0] = 0;
    char s[512], *p, *t;
    t = err_msg;
    while(fb_interpret(s,sizeof(s),&pvector)){
      for(p=s;(*p)&&((t-err_msg)<max_len);){ *t++ = *p++; }
      if((t-err_msg+1)<max_len) *t++='\n';
    };
    if((t-err_msg)<max_len) *t = 0;
    return err_msg;
}

#define MAX_ERR_MSG_LEN 1024
#define ERR_MSG(obj,class) \
String::New(ErrorMessage(obj->status,obj->err_message,sizeof(obj->err_message)))

#define REQ_EXT_ARG(I, VAR) \
if (args.Length() <= (I) || !args[I]->IsExternal()) \
return ThrowException(Exception::TypeError( \
String::New("Argument " #I " invalid"))); \
Local<External> VAR = Local<External>::Cast(args[I]);

#define MAX_EVENTS_PER_BLOCK	15
#define EXP_15_VAR_ARGS(x) x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7],x[8],x[9],x[10],x[11],x[12],x[13],x[14]

class FBResult : public EventEmitter {

public:

 static Persistent<FunctionTemplate> constructor_template;
 
 static void
   Initialize (v8::Handle<v8::Object> target)
  {
    HandleScope scope;
    
    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->Inherit(EventEmitter::constructor_template);
    constructor_template = Persistent<FunctionTemplate>::New(t);
    constructor_template->Inherit(EventEmitter::constructor_template);
    constructor_template->SetClassName(String::NewSymbol("FBResult"));

    Local<ObjectTemplate> instance_template =
        constructor_template->InstanceTemplate();
        
    NODE_SET_PROTOTYPE_METHOD(t, "fetchSync", FetchSync);
    NODE_SET_PROTOTYPE_METHOD(t, "fetch", Fetch);


    instance_template->SetInternalFieldCount(1);
    
    target->Set(String::NewSymbol("FBResult"), t->GetFunction());
  }
  
 bool process_result(XSQLDA **sqldap, isc_stmt_handle *stmtp, Local<Array> res)
  {
    int            fetch_stat;
    short 	   i, num_cols;	
    XSQLDA         *sqlda;
    
    uint32_t       j = 0;
    sqlda = *sqldap;
    if(!sqlda){ return true;}
    num_cols = (*sqldap)->sqld;
    
    HandleScope scope;
    
    Local<Object> js_result_row;
    Local<Value> js_field;
    
    while ((fetch_stat = isc_dsql_fetch(status, stmtp, SQL_DIALECT_V6, *sqldap)) == 0)
    {
        js_result_row = Array::New();
        for (i = 0; i < num_cols; i++)
        {
            js_field = FBResult::GetFieldValue((XSQLVAR *) &sqlda->sqlvar[i]);
            js_result_row->Set(Integer::NewFromUnsigned(i), js_field);
        }
        res->Set(Integer::NewFromUnsigned(j++), js_result_row);
    }
    return true;
  } 
  
protected:  
 static Handle<Value>
  New (const Arguments& args)
  {
  // XSQLDA **asqldap, isc_stmt_handle *astmtp
    HandleScope scope;
    
    REQ_EXT_ARG(0, js_sqldap);
    REQ_EXT_ARG(1, js_stmtp);
    
    XSQLDA *asqldap;
    asqldap = static_cast<XSQLDA *>(js_sqldap->Value());
    isc_stmt_handle *astmtp;
    astmtp = static_cast<isc_stmt_handle *>(js_stmtp->Value()); 
    
    FBResult *res = new FBResult(asqldap, astmtp);
    res->Wrap(args.This());

    return args.This();
  } 
  
  static Local<Value> 
  GetFieldValue(XSQLVAR *var)
  {
    short       dtype;  
    PARAMVARY   *vary2;
    short       len; 
    struct tm   times;
    ISC_QUAD    bid;
    time_t      rawtime;
    double      time_val;
    
    HandleScope scope;
    
    Local<Object> js_date;
    Local<Value> js_field = Local<Value>::New(Null());
    dtype = var->sqltype & ~1;
    if ((var->sqltype & 1) && (*var->sqlind < 0))
    {
     // NULL PROCESSING
    }
    else
    {
        switch (dtype)
        {
            case SQL_TEXT:
                js_field = String::New(var->sqldata,var->sqllen);
                break;

            case SQL_VARYING:
                vary2 = (PARAMVARY*) var->sqldata;
                vary2->vary_string[vary2->vary_length] = '\0';
                js_field = String::New((const char*)(vary2->vary_string));
                break;

            case SQL_SHORT:
            case SQL_LONG:
	    case SQL_INT64:
		{
		ISC_INT64	value;
		short		field_width;
		short		dscale;
		switch (dtype)
		    {
		    case SQL_SHORT:
			value = (ISC_INT64) *(short *) var->sqldata;
			field_width = 6;
			break;
		    case SQL_LONG:
			value = (ISC_INT64) *(int *) var->sqldata;
			field_width = 11;
			break;
		    case SQL_INT64:
			value = (ISC_INT64) *(ISC_INT64 *) var->sqldata;
			field_width = 21;
			break;
		    }
		ISC_INT64	tens;    
		short	i;
		dscale = var->sqlscale;
		
		if (dscale < 0)
		    {
		    tens = 1;
		    for (i = 0; i > dscale; i--) tens *= 10;
                    js_field = Number::New(value / tens); 
                    
		    }
		else if (dscale)
		      {
		       tens = 1;
		       for (i = 0; i < dscale; i++) tens *= 10;
		      js_field = Integer::New(value * tens); 
	              }		    
		else
		      js_field = Integer::New( value);
		}
                break;

            case SQL_FLOAT:
                js_field = Number::New(*(float *) (var->sqldata));  
                break;

            case SQL_DOUBLE:
                js_field = Number::New(*(double *) (var->sqldata));
                break;

            case SQL_TIMESTAMP: 
	            isc_decode_timestamp((ISC_TIMESTAMP *)var->sqldata, &times);
	            rawtime = mktime(&times);// + Connection::get_gmt_delta();
	            time_val = static_cast<double>(rawtime)*1000 + ((*((ISC_TIME *)var->sqldata)) % 10000)/10;
	            js_field = Date::New(time_val);
	            break;
	            
            case SQL_TYPE_DATE:    
	            	          
	            isc_decode_sql_date((ISC_DATE *)var->sqldata, &times);
	            rawtime = mktime(&times);// + Connection::get_gmt_delta();
	            js_date = Object::New();
	            js_date->Set(String::New("year"),
                             Integer::New(times.tm_year+1900));
                    js_date->Set(String::New("month"),
                             Integer::New(times.tm_mon+1));
                    js_date->Set(String::New("day"),
                             Integer::New(times.tm_mday));
	            time_val = static_cast<double>(rawtime)*1000;
	            js_date->Set(String::New("date"),
                             Date::New(time_val));
	            js_field = js_date;
	            break;
	            
           case SQL_TYPE_TIME:    
	            isc_decode_sql_time((ISC_TIME *)var->sqldata, &times);
	            time_val = (times.tm_hour*60*60 + 
	                        times.tm_min*60 +
	                        times.tm_sec)*1000 + 
	                        ((*((ISC_TIME *)var->sqldata)) % 10000)/10;
	            js_field = Date::New(time_val);            
	            break;            

            case SQL_BLOB:
            case SQL_ARRAY:
                /* Print the blob id on blobs or arrays */
                Local<Object> js_blob;
                bid = *(ISC_QUAD *) var->sqldata;
                js_blob = Object::New();
                js_blob->Set(String::New("q_hi"),
                             Integer::New(bid.gds_quad_high));
                js_blob->Set(String::New("q_lo"),
                             Integer::NewFromUnsigned(bid.gds_quad_low));             

                js_field = js_blob;
                
                break;

        }

    }
    
    return scope.Close(js_field);
  }

  static Handle<Value>
  FetchSync(const Arguments& args) 
  {
    HandleScope scope;
    
    FBResult *fb_res = ObjectWrap::Unwrap<FBResult>(args.This());
    
    int            fetch_stat;
    short 	   i, num_cols;	
    XSQLDA         *sqlda;
    
    uint32_t       j = 0;
    sqlda = fb_res->sqldap;
    if(!sqlda){ return Undefined();}
    num_cols = sqlda->sqld;
    
    Local<Value> js_result = Local<Value>::New(Null());
    
    if (args.Length() < 2){
       return ThrowException(Exception::Error(
            String::New("Expecting 2 arguments")));
    }
    
    int rowCount = -1;
    if(args[0]->IsInt32()){
       rowCount = args[0]->IntegerValue();
    }
    else if(! (args[0]->IsString() && args[0]->Equals(String::New("all")))){
       return ThrowException(Exception::Error(
            String::New("Expecting integer or string as first argument")));
    };
    if(rowCount<=0) rowCount = -1;
    
    bool rowAsObject = false;
    if(args[1]->IsBoolean()){
         rowAsObject = args[1]->BooleanValue();
    }else if(args[1]->IsString() && args[1]->Equals(String::New("array"))){
         rowAsObject = false;
    }else if(args[1]->IsString() && args[1]->Equals(String::New("object"))){
         rowAsObject = true;
    } else{
     return ThrowException(Exception::Error(
            String::New("Expecting bool or string('array'|'object') as second argument")));
    };   
    
    Local<Value> js_field;
    Local<Object> js_result_row;   
        
    Local<Array> res = Array::New(); 
    while (((fetch_stat = isc_dsql_fetch(fb_res->status, &fb_res->stmt, SQL_DIALECT_V6, sqlda)) == 0)&&((rowCount==-1)||(rowCount>0)))
    {
        //js_result_row = Array::New();
         if(rowAsObject)
            js_result_row = Object::New();
         else 
            js_result_row = Array::New();
        
        for (i = 0; i < num_cols; i++)
        {
            js_field = FBResult::GetFieldValue((XSQLVAR *) &sqlda->sqlvar[i]);
            if(rowAsObject)
            { 
              js_result_row->Set(String::New(sqlda->sqlvar[i].sqlname), js_field);
            }
            else
            js_result_row->Set(Integer::NewFromUnsigned(i), js_field);
        }
        res->Set(Integer::NewFromUnsigned(j++), js_result_row);
        if(rowCount>0) rowCount--;
    }
    
    if ((fetch_stat != 100L) && fetch_stat) 
          return ThrowException(Exception::Error(
            String::Concat(String::New("While FetchSync - "),ERR_MSG(fb_res,FBResult))));

    
//    if(j==1) js_result = res->Get(0);
//    else 
     js_result = res;
    
    return scope.Close(js_result);

  }

  struct fetch_request {
     Persistent<Value> rowCallback;
     Persistent<Function> eofCallback;
     FBResult *res;
     int rowCount;
     int fetchStat;
     bool rowAsObject;
  };
  
  static int EIO_After_Fetch(eio_req *req)
  {
    ev_unref(EV_DEFAULT_UC);
    HandleScope scope;
    struct fetch_request *f_req = (struct fetch_request *)(req->data);
    short i, num_cols;
    num_cols = f_req->res->sqldap->sqld;
    
    Local<Value> js_field;
    Local<Object> js_result_row;   
    Local<Value> argv[3];
    int argc = 0;
    
    if(req->result)
    {
        if(f_req->rowCount>0) f_req->rowCount--;  
        
	if(f_req->rowAsObject)
    	    js_result_row = Object::New();
	else 
    	    js_result_row = Array::New();
    
	for (i = 0; i < num_cols; i++)
	{
    	    js_field = FBResult::GetFieldValue((XSQLVAR *) &f_req->res->sqldap->sqlvar[i]);
    	    if(f_req->rowAsObject)
    	    { 
        	js_result_row->Set(String::New(f_req->res->sqldap->sqlvar[i].sqlname), js_field);
    	    }
    	    else
        	js_result_row->Set(Integer::NewFromUnsigned(i), js_field);
        }
        
        if(f_req->rowCallback->IsFunction()){
    	    argv[0] = js_result_row;
        
	    TryCatch try_catch;

	    Local<Value> ret = Persistent<Function>::Cast(f_req->rowCallback)->Call(Context::GetCurrent()->Global(), 1, argv);

	    if (try_catch.HasCaught()) {
    		node::FatalException(try_catch);
	    }
	    else
	    if((!ret->IsBoolean() || ret->BooleanValue())&&f_req->rowCount!=0)
	    {
		eio_custom(EIO_Fetch, EIO_PRI_DEFAULT, EIO_After_Fetch, f_req);
        	ev_ref(EV_DEFAULT_UC);
        	return 0;
	    }
	    else
	    {
	      argc = 2;
	      argv[0] = Local<Value>::New(Null());
	      argv[1] = Local<Value>::New(False());
	    }
	}
	else
	{ 
	 /* TODO: 
	 *   accumulate result here 
	 */
	}
    }
    else 
    if(f_req->fetchStat!=100L){
          argc = 1;
          argv[0] = Exception::Error(
            String::Concat(String::New("While connecting - "),ERR_MSG(f_req->res,FBResult)));
    }
    else
    {
          argc = 2;
          argv[0] = Local<Value>::New(Null());
          argv[1] = Local<Value>::New(True());
    }

   
    TryCatch try_catch;
    f_req->eofCallback->Call(Context::GetCurrent()->Global(), argc, argv);

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
    

    f_req->rowCallback.Dispose();
    f_req->eofCallback.Dispose();
    f_req->res->Unref();
    free(f_req);

    return 0;
  } 

  static int EIO_Fetch(eio_req *req)
  {
    struct fetch_request *f_req = (struct fetch_request *)(req->data);
    
    f_req->fetchStat = isc_dsql_fetch(f_req->res->status, &f_req->res->stmt, SQL_DIALECT_V6, f_req->res->sqldap);
    
    req->result = (f_req->fetchStat == 0);

    return 0;
  }
  
  static Handle<Value>
  Fetch(const Arguments& args) 
  {
    HandleScope scope;
    FBResult *res = ObjectWrap::Unwrap<FBResult>(args.This());
    
    struct fetch_request *f_req =
         (struct fetch_request *)calloc(1, sizeof(struct fetch_request));

    if (!f_req) {
      V8::LowMemoryNotification();
      return ThrowException(Exception::Error(
            String::New("Could not allocate memory.")));
    }
    
    if (args.Length() != 4){
       return ThrowException(Exception::Error(
            String::New("Expecting 4 arguments")));
    }
    
    f_req->rowCount = -1;
    if(args[0]->IsInt32()){
       f_req->rowCount = args[0]->IntegerValue();
    }
    else if(! (args[0]->IsString() && args[0]->Equals(String::New("all")))){
       return ThrowException(Exception::Error(
            String::New("Expecting integer or string as first argument")));
    };
    if(f_req->rowCount<=0) f_req->rowCount = -1;
    
    f_req->rowAsObject = false;
    if(args[1]->IsBoolean()){
         f_req->rowAsObject = args[1]->BooleanValue();
    }else if(args[1]->IsString() && args[1]->Equals(String::New("array"))){
         f_req->rowAsObject = false;
    }else if(args[1]->IsString() && args[1]->Equals(String::New("object"))){
         f_req->rowAsObject = true;
    } else{
     return ThrowException(Exception::Error(
            String::New("Expecting bool or string('array'|'object') as second argument")));
    };
    
/*    if(!args[2]->IsFunction())  f_req->rowCallback = Persistent<Value>::New(Null());
    else  f_req->rowCallback = Persistent<Function>::New(Local<Function>::Cast(args[2]));
*/   
    f_req->rowCallback  = Persistent<Value>::New(args[2]);
    
    if(!args[3]->IsFunction()) {
      return ThrowException(Exception::Error(
            String::New("Expecting Function as fourth argument")));
    }

    
    f_req->res = res;
    f_req->eofCallback = Persistent<Function>::New(Local<Function>::Cast(args[3]));
    
    eio_custom(EIO_Fetch, EIO_PRI_DEFAULT, EIO_After_Fetch, f_req);
    
    ev_ref(EV_DEFAULT_UC);
    res->Ref();
    
    return Undefined();
  }

  
   FBResult (XSQLDA *asqldap, isc_stmt_handle *astmtp) : EventEmitter () 
  {
    sqldap = asqldap;
    stmt = *astmtp;
  }
  
  ~FBResult()
  {
   if(sqldap) {
     free(sqldap);
   }
   
  }
  
  ISC_STATUS_ARRAY status;
  char err_message[MAX_ERR_MSG_LEN];    
  XSQLDA *sqldap;
  isc_stmt_handle stmt;
   
};

/*
*	Class event_block 
*   	Firebird accepts events in blocks by 15 event names.
*	This class helps to organize linked list chain of such blocks
*	to support "infinite" number of events.
*/
typedef char* ev_names[MAX_EVENTS_PER_BLOCK];
static Persistent<String> fbevent_symbol;

class event_block {

public:
    Connection *conn;
    ISC_UCHAR *event_buffer;
    ISC_UCHAR *result_buffer;
    ISC_LONG event_id;
    ev_names event_names;
    int count, reg_count, del_count;
    long blength;
    event_block *next, *prev;
    ev_async *event_;
    
    
    ISC_STATUS_ARRAY status;
    char err_message[MAX_ERR_MSG_LEN];
    ISC_STATUS_ARRAY Was_reg, deleted;
    isc_db_handle *db;
    bool first_time, bad, canceled, lock;
    
    void dump_buf(char* buf, int len){
      printf("buff dump %d\n",len);
      printf("buff[0] = %d\n", buf[0]);
      buf++; len--;
      int c;
      char curr_name[127];
      char* cn;
      while(len > 0){
        c = (unsigned char) buf[0];
        printf("name len = %d\n",c);
        buf++;len--;
        for(cn = curr_name;c--;len--){
          *cn++ = *buf++;
        }
        *cn++ = 0;
        printf("event_name = %s\n",curr_name);
        printf("count = %d\n",(uint32_t) *buf);
        len = len - sizeof(uint32_t);
        buf = buf + sizeof(uint32_t);
      }
    }
    
    void set_counts(char* buf, int len){
      buf++;len--;
      int c;
      while(len>0){
        c = (unsigned char) buf[0];
        buf = buf + c  + 1;len =  len - c - 1;
        *(buf++) = -1; *(buf++) = -1; *(buf++) = -1; *(buf++) = -1;
        len = len - sizeof(uint32_t);
      }
    }
    
    void get_counts(ISC_ULONG* Vector){
       char *eb, *rb;
       long len = blength;
       eb = (char*) event_buffer;
       rb = (char*) result_buffer;
       int idx = 0;
       eb++;rb++;len--;
       int c;
       uint32_t vold;
       uint32_t vnew;
       while(len){
         c = (unsigned char) rb[0];
         eb =   eb  + c + 1;
         rb =   rb  + c + 1;
         len =  len - c - 1;
         len = len - sizeof(uint32_t);
         
         vnew = (uint32_t) *rb;
         vold = (uint32_t) *eb;
         eb = eb + 4;
         rb = rb + 4;
         if(vnew > vold){
           Vector[idx] = (int) vnew - vold;
         }
         else Vector[idx] = 0;
         
         idx++;
       }
       memcpy(event_buffer,result_buffer,blength);
    }
    
    bool alloc_block()
    {
      // allocate event buffers
      // we pass all event_names as there is no method to pass varargs dynamically
      // isc_event_block will not use more than count
      blength = isc_event_block(&event_buffer, &result_buffer, reg_count, EXP_15_VAR_ARGS(event_names));
      set_counts((char*) event_buffer,blength);
      memcpy(result_buffer,event_buffer,blength);
      
//      printf("-->event_buf\n");
//      dump_buf((char*) event_buffer,blength);
//      printf("-->result_buf\n");
//      dump_buf((char*) result_buffer,blength);

      return event_buffer && result_buffer && blength;
    }
    
    bool cancel_events() // or free_block ?
    {
      // If event_id was set - we have queued events
      // so cancel them
      
      if(event_id)
      {
       canceled = true;
       if(isc_cancel_events(status, db, &event_id)) return false; 
       event_id = 0;
      } 
      // free buffers allocated by isc_event_block
      if(blength)
      { 
       isc_free((ISC_SCHAR*) event_buffer);
       event_buffer  = NULL;
       isc_free((ISC_SCHAR*) result_buffer);
       result_buffer  = NULL;
       blength = 0;
      }
      return true;
    }
    
    static void isc_ev_callback(void *aeb, ISC_USHORT length, const ISC_UCHAR *updated)
    {
      // this callback is called by libfbclient when event occures in FB
      
      event_block* eb = static_cast<event_block*>(aeb);
      
      // have not found that in documentation
      // but on isc_cancel_events this callback is called
      // with updated == NULL, length==0
      // here we have potential race condition 
      // when cancel and callback get called simultanously .. hmm..
      if(!updated || eb->canceled || !length) {
      // in that case mark block as bad and exit;
        eb->bad = true;
        return;
      }     
      // mark block as locked 
      // to prevent reallocation of buffers when adding or deleting records to block
      // potential race condition ????
      eb->lock = true;
      eb->bad = false;
      
      // copy updated to result buffer
      // or examples use loop
      // why not to use mmcpy ???
//      printf("-->result_buf in call back !\n");
//      eb->dump_buf((char*) updated,length);

      ISC_UCHAR *r = eb->result_buffer;
      while(length--) *r++ = *updated++;

      
      // schedule event_notification call
      ev_async_send(EV_DEFAULT_UC_ eb->event_);
    }
    
    static void event_notification(EV_P_ ev_async *w, int revents){
        ISC_STATUS_ARRAY Vector;
        bool use_vector = true;
        HandleScope scope;
	Local<Value> argv[2];
	
        event_block* eb = static_cast<event_block*>(w->data);
        
        // get event counts
        // but only if block is not marked as bad
        if(!eb->bad)
           eb->get_counts((ISC_ULONG*) Vector);
	 //isc_event_counts((ISC_ULONG*) Vector, eb->blength, eb->event_buffer,
	//			      eb->result_buffer);
	else 
	{
	 // do not use vector 
	 // if block is bad
	 use_vector = false;
	} 
			
	// we have event counts
	// so we may unlock block
	// race???			      
	eb->lock  = false;
	
	// use only previously queued events
	int count = eb->reg_count;
	int i;
	Local<Value> EvNames[MAX_EVENTS_PER_BLOCK];

	// update Events Counts Vector
	// when event first registered it is initialized with count = 1
	// But if you register and than unregister event
	// and after that register it again if will be inited with count = 2
	// now it does not track event registration history
	// may be it should ???
	// all that "tracking" is reset on disconnect
	if(use_vector)
	for(i=0; i < count; i++){
	  if((Vector[i] - eb->Was_reg[i])>0) EvNames[i] = String::New(eb->event_names[i]);
	  if(eb->Was_reg[i]){
	    Vector[i]--;
	    if(Vector[i]<0) Vector[i] = 0;
	    else  eb->Was_reg[i] = 0;      
	  }    
	}
	
	// detect if we have added or deleted some events
	// while block was locked
        if((eb->count!=eb->reg_count) || eb->del_count){
        
    	    // if so - free buffers
    	    eb->event_id = 0;
    	    if(!eb->cancel_events()) {
    	      ThrowException(Exception::Error(
               String::Concat(String::New("While cancel_events - "),ERR_MSG(eb, event_block))));
               return ;
    	    }     
    	    // update event list
    	    if(eb->del_count) eb->removeDeleted();
    	    eb->reg_count = eb->count;     
            
            // allocate new events
    	    if(!eb->alloc_block()) {
    		V8::LowMemoryNotification();
    		ThrowException(Exception::Error(
			      	    String::New("Could not allocate event block buffers.")));
	        return ;
    	    }
    	    // set initial event count 
            //for(i=0; i<eb->count; i++) eb->Was_reg[i] = 1;	    
        }

        // re queue event trap
	if(isc_que_events(
    	    eb->status,
            eb->db,
            &(eb->event_id),
            eb->blength,
            eb->event_buffer,
            event_block::isc_ev_callback,
            eb          
        )) {
            ThrowException(Exception::Error(
            String::Concat(String::New("While isc_que_events - "),ERR_MSG(eb, event_block))));
        }			      
        
        // exit if block was marked as bad
        
        if(!use_vector) return;
	Connection *conn = eb->conn;

        // emit events 
	for( i=0; i < count; i++){
	   if(Vector[i]) {
	     argv[0] = EvNames[i];
	     argv[1] = Integer::New(Vector[i]);
             ((EventEmitter*) conn)->Emit(fbevent_symbol,2,argv);
           }     
        }

    }
    
    static Handle<Value> 
    que_event(event_block *eb)
    { 
    
      if(!eb->alloc_block()) {
    	    V8::LowMemoryNotification();
    	    return ThrowException(Exception::Error(
			      	  String::New("Could not allocate event block buffers.")));
      }
     
      eb->canceled = false;
      //for(int i=0; i<eb->count; i++) eb->Was_reg[i] = 1;
      if(isc_que_events(
         eb->status,
         eb->db,
         &(eb->event_id),
         eb->blength,
         eb->event_buffer,
         event_block::isc_ev_callback,
         eb          
      )) {
            return ThrowException(Exception::Error(
            String::Concat(String::New("While isc_que_events - "),ERR_MSG(eb, event_block))));
      }
      return Undefined();
    }
    
    int hasEvent(char *Event)
    {
      int i;
      for(i=0;i<count;i++){
       if(event_names[i]&&(strcmp(event_names[i],Event)==0)) return i;
      } 
      return -1;
    }
    
    bool addEvent(char *Event)
    {
      // allocate memory for event name
      int len = strlen(Event);
      event_names[count] = (char*) calloc(1,len+1);
      if(!event_names[count]) return false;
      
      
      // copy event name
      char *p, *t;
      t = event_names[count];
      for(p=Event;(*p);){ *t++ = *p++; }
      *t = 0;
      
      count++;
      
      // if block is locked 
      // just add event_name
      // but delay registration 
      // until notification is called
      if(!lock) reg_count = count; 
      
      return true;
    }
    
    void removeEvent(char *Event)
    { 
      int idx = hasEvent(Event); 
      if(idx<0) return;
      // delete event if block is not locked
      // or if deleted event was not registered yet
      if(!lock || (idx>=reg_count)){
        free(event_names[idx]);
        for(int i = idx;i < count;i++) event_names[i] = event_names[i+1];
        reg_count--;
        count--;
      }
      else{
       // if block is locked mark events to be deleted
       // when notification will be called
        for(int i = 0; i<del_count; i++) 
         if(deleted[i]==idx) return ;
        deleted[del_count] = idx;
        del_count++;
      }
          
    }
    
    // removes events to be deleted
    void removeDeleted()
    {
      int i,j;
      for(i=0; i<del_count; i++){
        free(event_names[i]);
        event_names[i] = NULL;
      }
      i=0;
      while(i<count){
         if(!event_names[i]){ 
           for(j=i;j<count;j++) event_names[j] = event_names[j+1];
           count--;
         }  
         i++;
      }
      del_count = 0;
    }
    
    static Handle<Value> 
    RegEvent(event_block** rootp, char *Event, Connection *aconn, isc_db_handle *db)
    {
      event_block* root = *rootp;
      // Check if we already have registered that event
      event_block* res = event_block::FindBlock(root,Event);
      // Exit if yes
      if(res) return Undefined();
  
      // Search for available event block
      res = root;
      while(res)
      {
        if(res->count < MAX_EVENTS_PER_BLOCK) break;
        root = res;
        res = res->next;
      }
      // Have we found one ?
      if(!res){
        // Create new event block
        res = new event_block(aconn, db);
        if(!res){
    	    V8::LowMemoryNotification();
    	    return ThrowException(Exception::Error(
			      	  String::New("Could not allocate memory.")));
	}
        res->event_->data = res;
        ev_async_init(res->event_, event_block::event_notification);
        ev_async_start(EV_DEFAULT_UC_ res->event_);
        ev_unref(EV_DEFAULT_UC);
        
        // Set links
        if(root) root->next = res;
        res->prev = root;
      }
      
      // Set first element in chain if it is not set yet
      if(!*rootp) *rootp = res;
      
      
      if(res->lock){
        // if block is locked
        // just add event name and exit
        if(!res->addEvent(Event)) {
    	    V8::LowMemoryNotification();
    	    return ThrowException(Exception::Error(
			      	  String::New("Could not allocate memory.")));
        }
        return Undefined();
      }
      
      // Cancel old queue is any 
      if(!res->cancel_events()){
    	    return ThrowException(Exception::Error(
        	String::Concat(String::New("While cancel_events - "),ERR_MSG(res, event_block))));
      }        

      // Add event to block
      if(!res->addEvent(Event)) {
    	    V8::LowMemoryNotification();
    	    return ThrowException(Exception::Error(
			      	  String::New("Could not allocate memory.")));
      }

      return event_block::que_event(res); 

    }
    
    static event_block* FindBlock(event_block* root, char *Event)
    {
      // Find block with Event in linked list
      event_block* res = root;
      while(res)
       if(res->hasEvent(Event)==-1) res = res->next;
       else break;
      return res;
    }
    
    static Handle<Value> 
    RemoveEvent(event_block** root, char *Event)
    {
      // Find event_block with Event name
      event_block* eb = event_block::FindBlock( *root, Event);
      if(eb)
      {
        // If we have found it
        // it was queued and should be canceled
        if(!eb->cancel_events()){
    	    return ThrowException(Exception::Error(
        	    String::Concat(String::New("While cancel_events - "),ERR_MSG(eb, event_block))));
    	}        
    	// Remove it from event list
        eb->removeEvent(Event);
        if(eb->lock) return Undefined();
        if(!eb->count) {
         // If there is no more events in block
         // we can free it
         // but keep link to next block
         if(eb->prev) 
         {
          eb->prev->next = eb->next;
         }
         else
         {
          *root = NULL;
         }
         eb->next = NULL;
         free(eb);
        } 
        else {
            // if block still has events we should requeue it
    	    return event_block::que_event(eb);    
        }
      }	
      // No block with that name 
      // may be throw error ???
      return Undefined();
    }
    
    event_block(Connection *aconn,isc_db_handle *adb)
    { 
      conn = aconn;
      db = adb;
      count = 0;
      reg_count = 0;
      del_count = 0;
      next = NULL;
      prev = NULL;
      event_buffer = NULL;
      result_buffer = NULL;
      blength = 0;
      event_id = 0;
      event_ = new ev_async();
      first_time = true;
      bad = false;
      canceled = false;
      lock = false;
      memset(Was_reg,0,sizeof(Was_reg));
    }
    
    ~event_block()
    {
      cancel_events();
      for(int i=0;i<count;i++) free(event_names[i]);
      free(event_);
      if(next) free(next);
    }
};


Persistent<FunctionTemplate> FBResult::constructor_template;

class Connection : public EventEmitter {
 public:
  static void
  Initialize (v8::Handle<v8::Object> target)
  {
    HandleScope scope;
    
    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    
    t->Inherit(EventEmitter::constructor_template);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    // Methods 

    NODE_SET_PROTOTYPE_METHOD(t, "connectSync", ConnectSync);
    NODE_SET_PROTOTYPE_METHOD(t, "connect", Connect);
    NODE_SET_PROTOTYPE_METHOD(t, "querySync", QuerySync);
    NODE_SET_PROTOTYPE_METHOD(t, "query", Query);
    NODE_SET_PROTOTYPE_METHOD(t, "disconnect", Disconnect);
    NODE_SET_PROTOTYPE_METHOD(t, "commitSync", CommitSync);
    NODE_SET_PROTOTYPE_METHOD(t, "commit", Commit);
    NODE_SET_PROTOTYPE_METHOD(t, "rollbackSync", RollbackSync);
    NODE_SET_PROTOTYPE_METHOD(t, "rollback", Rollback);
    NODE_SET_PROTOTYPE_METHOD(t, "addFBevent", addEvent);
    NODE_SET_PROTOTYPE_METHOD(t, "deleteFBevent", deleteEvent);
    
    // Properties
    Local<v8::ObjectTemplate> instance_t = t->InstanceTemplate();
    instance_t->SetAccessor(String::NewSymbol("connected"), ConnectedGetter);
    instance_t->SetAccessor(String::NewSymbol("inTransaction"), InTransactionGetter);
    
    fbevent_symbol = NODE_PSYMBOL("fbevent");
    
    target->Set(String::NewSymbol("Connection"), t->GetFunction());  
  }
  
  bool Connect (const char* Database,const char* User,const char* Password,const char* Role)
  {
    if (db) return false;
    int i = 0, len;
    char dpb[256];
    char *lc_type = "UTF8";
    
    dpb[i++] = isc_dpb_version1;
    
    dpb[i++] = isc_dpb_user_name;
    len = strlen (User);
    dpb[i++] = (char) len;
    strncpy(&(dpb[i]), User, len);
    i += len;
    
    dpb[i++] = isc_dpb_password;
    len = strlen (Password);
    dpb[i++] = len;
    strncpy(&(dpb[i]), Password, len);
    i += len;

    dpb[i++] = isc_dpb_sql_role_name;
    len = strlen (Role);
    dpb[i++] = len;
    strncpy(&(dpb[i]), Role, len);
    i += len;
    
    dpb[i++] = isc_dpb_lc_ctype;
    len = strlen (lc_type);
    dpb[i++] = len;
    strncpy(&(dpb[i]), lc_type, len);
    i += len;
    
    connected = false;
    if(isc_attach_database(status, 0, Database, &(db), i, dpb)) return false;
    connected = true;
    return true;
  } 
  
  
  bool Close(){
    
    if(trans)
     if(!commit_transaction()) {
       return false;
     }
    
    if(fb_events) free(fb_events);
    fb_events = NULL;
 
    connected = false; 
    if (isc_detach_database(status, &db)) {
      db = NULL;
      return false;
    } 
    db = NULL;
    return true;
    
  }
  
  bool process_statement(XSQLDA **sqldap, char *query, isc_stmt_handle *stmtp)
  {
     int            buffer[1024];
     XSQLDA          *sqlda;
     XSQLVAR         *var;
     static char     stmt_info[] = { isc_info_sql_stmt_type };
     char            info_buffer[20];
     short           l, num_cols, i, length, alignment, type, offset;
     int             statement_type;
     
     
     sqlda = *sqldap;
     
     if(!sqlda)
     {
          sqlda = (XSQLDA *) malloc(XSQLDA_LENGTH (20));
          sqlda->sqln = 20;
          sqlda->version = 1;
          *sqldap = sqlda;
     }      

     /* Allocate a statement */
     if(!*stmtp) {
      if (isc_dsql_allocate_statement(status, &db, stmtp)) return false;
     } 
     
     // Start Default Transaction If None Active
     if(!trans) 
      {
      if (isc_start_transaction(status, &trans, 1, &db, 0, NULL)) return false;
      }
      
     // Prepare Statement
     if (isc_dsql_prepare(status, &trans, stmtp, 0, query, SQL_DIALECT_V6, sqlda)) return false;

     // Get sql info
     if (!isc_dsql_sql_info(status, stmtp, sizeof (stmt_info), stmt_info, 
         sizeof (info_buffer), info_buffer))
     {
         l = (short) isc_vax_integer((char *) info_buffer + 1, 2);
         statement_type = isc_vax_integer((char *) info_buffer + 3, l);
     }
     
     // It is statement w\o resultset
     if (!sqlda->sqld)
     {
        
        free(sqlda);
        *sqldap = NULL;
                   
        if (isc_dsql_execute(status, &trans, stmtp, SQL_DIALECT_V6, NULL))
        {
             return false;
        }

        /* Commit DDL statements if that is what sql_info says */

        if (trans && (statement_type == isc_info_sql_stmt_ddl))
        {
            //printf("it is ddl !");
            if (isc_commit_transaction(status, &trans))
            {
             trans = 0;
             return false;    
            }    
            trans = 0;
        }

        return true;
     }

     /*
      *    Process select statements.
      */

     num_cols = sqlda->sqld;

     /* Need more room. */
     if (sqlda->sqln < num_cols)
     {
        *sqldap = sqlda = (XSQLDA *) realloc(sqlda,
                                                XSQLDA_LENGTH (num_cols));
        sqlda->sqln = num_cols;
        sqlda->version = 1;

        if (isc_dsql_describe(status, stmtp, SQL_DIALECT_V6, sqlda))
        {
            return false;
        }

        num_cols = sqlda->sqld;
     }

     /*
      *     Set up SQLDA.
      */
     for (var = sqlda->sqlvar, offset = 0, i = 0; i < num_cols; var++, i++)
     {
        length = alignment = var->sqllen;
        type = var->sqltype & ~1;

        if (type == SQL_TEXT)
            alignment = 1;
        else if (type == SQL_VARYING)
        {   
            length += sizeof (short) + 1;
            alignment = sizeof (short);
        }
        /* Comment from firebird sample: 
        **  RISC machines are finicky about word alignment
        **  So the output buffer values must be placed on
        **  word boundaries where appropriate
        */
        offset = FB_ALIGN(offset, alignment);
        var->sqldata = (char *) buffer + offset;
        offset += length;
        offset = FB_ALIGN(offset, sizeof (short));
        var->sqlind = (short*) ((char *) buffer + offset);
        offset += sizeof  (short);
     }

    if (isc_dsql_execute(status, &trans, stmtp, SQL_DIALECT_V6, NULL))
    {
        return false;
    }
    return true;

  }
  

  
  bool commit_transaction()
  {
    if (isc_commit_transaction(status, &trans))
    {
             trans = 0;
             return false;    
    }    
    trans = 0;
    return true;
  }
    
  bool rollback_transaction()
  {
    if (isc_rollback_transaction(status, &trans))
    {
             trans = 0;
             return false;    
    }    
    trans = 0;
    return true;
  }
  
  
  
 protected:
 
 static Handle<Value>
  New (const Arguments& args)
  {
    HandleScope scope;

    Connection *connection = new Connection();
    connection->Wrap(args.This());

    return args.This();
  }
  
  static Handle<Value>
  ConnectSync (const Arguments& args)
  {
    Connection *connection = ObjectWrap::Unwrap<Connection>(args.This());

    HandleScope scope;

    if (args.Length() < 4 || !args[0]->IsString() ||
         !args[1]->IsString() || !args[2]->IsString() ||
         !args[3]->IsString()) {
      return ThrowException(Exception::Error(
            String::New("Expecting 4 string arguments")));
    }

    String::Utf8Value Database(args[0]->ToString());
    String::Utf8Value User(args[1]->ToString());
    String::Utf8Value Password(args[2]->ToString());
    String::Utf8Value Role(args[3]->ToString());
    

    bool r = connection->Connect(*Database,*User,*Password,*Role);

    if (!r) {
      return ThrowException(Exception::Error(
            String::Concat(String::New("While connecting - "),ERR_MSG(connection, Connection))));
    }
    
    return Undefined();
  }

  struct connect_request {
     Persistent<Function> callback;
     Connection *conn;
     String::Utf8Value *Database;
     String::Utf8Value *User;
     String::Utf8Value *Password;
     String::Utf8Value *Role;
     bool res;
  };
  
  static int EIO_After_Connect(eio_req *req)
  {
    ev_unref(EV_DEFAULT_UC);
    HandleScope scope;
    struct connect_request *conn_req = (struct connect_request *)(req->data);

    Local<Value> argv[1];
    
    if (!req->result) {
       argv[0] = Exception::Error(
            String::Concat(String::New("While connecting - "),ERR_MSG(conn_req->conn, Connection)));
    }
    else{
     argv[0] = Local<Value>::New(Null());
    }
   
    TryCatch try_catch;

    conn_req->callback->Call(Context::GetCurrent()->Global(), 1, argv);

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }

    conn_req->callback.Dispose();
    conn_req->conn->Unref();
    free(conn_req);

    return 0;
    
  }
  
  static int EIO_Connect(eio_req *req)
  {
    struct connect_request *conn_req = (struct connect_request *)(req->data);
    req->result = conn_req->conn->Connect(
                     **(conn_req->Database),
                     **(conn_req->User), 
                     **(conn_req->Password),
                     **(conn_req->Role));
    conn_req->res = req->result;
    delete conn_req->Database;
    delete conn_req->User;
    delete conn_req->Password;
    delete conn_req->Role;
    
    return 0;
   
  }
  
  static Handle<Value>
  Connect (const Arguments& args)
  {
    HandleScope scope;
    Connection *conn = ObjectWrap::Unwrap<Connection>(args.This());
    
    struct connect_request *conn_req =
         (struct connect_request *)calloc(1, sizeof(struct connect_request));

    if (!conn_req) {
      V8::LowMemoryNotification();
      return ThrowException(Exception::Error(
            String::New("Could not allocate memory.")));
    }
    
    if (args.Length() < 5 || !args[0]->IsString() ||
         !args[1]->IsString() || !args[2]->IsString() ||
         !args[3]->IsString() || !args[4]->IsFunction()) {
      return ThrowException(Exception::Error(
            String::New("Expecting 4 string arguments and 1 Function")));
    }
    
    conn_req->conn = conn;
    conn_req->callback = Persistent<Function>::New(Local<Function>::Cast(args[4]));
    conn_req->Database = new String::Utf8Value(args[0]->ToString());
    conn_req->User     = new String::Utf8Value(args[1]->ToString());
    conn_req->Password = new String::Utf8Value(args[2]->ToString());
    conn_req->Role     = new String::Utf8Value(args[3]->ToString());
    
    eio_custom(EIO_Connect, EIO_PRI_DEFAULT, EIO_After_Connect, conn_req);
    
    ev_ref(EV_DEFAULT_UC);
    conn->Ref();
    
    return Undefined();
  }
  
  static Handle<Value>
  Disconnect(const Arguments& args)
  {
    Connection *connection = ObjectWrap::Unwrap<Connection>(args.This());

    HandleScope scope;
    //printf("disconnect\n");
    if(!connection->Close()){
      return ThrowException(Exception::Error(
            String::Concat(String::New("While closing - "),ERR_MSG(connection, Connection))));
    }     
   
    return Undefined();
  }
  
  static Handle<Value> ConnectedGetter(Local<String> property,
                                      const AccessorInfo &info) {
    HandleScope scope;
    Connection *connection = ObjectWrap::Unwrap<Connection>(info.Holder());

    return scope.Close(Boolean::New(connection->connected));
  }
  
  static Handle<Value>
  InTransactionGetter(Local<String> property,
                      const AccessorInfo &info) 
  {
    HandleScope scope;
    Connection *connection = ObjectWrap::Unwrap<Connection>(info.Holder());

    return scope.Close(Boolean::New(connection->trans));
  }

  static Handle<Value>
  CommitSync (const Arguments& args)
  {
    Connection *connection = ObjectWrap::Unwrap<Connection>(args.This());

    HandleScope scope;
    
    bool r = connection->commit_transaction();

    if (!r) {
      return ThrowException(Exception::Error(
            String::Concat(String::New("While commitSync - "),ERR_MSG(connection, Connection))));
    }
    
    return Undefined();
  } 
    
  static Handle<Value>
  RollbackSync (const Arguments& args)
  {
    Connection *connection = ObjectWrap::Unwrap<Connection>(args.This());

    HandleScope scope;
    
    bool r = connection->rollback_transaction();

    if (!r) {
      return ThrowException(Exception::Error(
            String::Concat(String::New("While rollbackSync - "),ERR_MSG(connection, Connection))));
    }
    
    return Undefined();
  } 
  
  struct transaction_request {
     Persistent<Function> callback;
     Connection *conn;
     bool commit;
  };

  static int EIO_After_TransactionRequest(eio_req *req)
  {
    ev_unref(EV_DEFAULT_UC);
    HandleScope scope;
    struct transaction_request *tr_req = (struct transaction_request *)(req->data);

    Local<Value> argv[1];
    
    if (!req->result) {
       argv[0] = Exception::Error(ERR_MSG(tr_req->conn, Connection));
    }
    else{
     argv[0] = Local<Value>::New(Null());
    }
   
    TryCatch try_catch;

    tr_req->callback->Call(Context::GetCurrent()->Global(), 1, argv);

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }

    tr_req->callback.Dispose();
    tr_req->conn->Unref();
    free(tr_req);

    return 0;
    
  }
  
  static int EIO_TransactionRequest(eio_req *req)
  {
    struct transaction_request *tr_req = (struct transaction_request *)(req->data);
    if(tr_req->commit) req->result = tr_req->conn->commit_transaction();
    else req->result = tr_req->conn->rollback_transaction();
    return 0;
  }

  
  static Handle<Value>
  Commit (const Arguments& args)
  {
    HandleScope scope;
    Connection *conn = ObjectWrap::Unwrap<Connection>(args.This());
    
    struct transaction_request *tr_req =
         (struct transaction_request *)calloc(1, sizeof(struct transaction_request));

    if (!tr_req) {
      V8::LowMemoryNotification();
      return ThrowException(Exception::Error(
            String::New("Could not allocate memory.")));
    }
    
    if (args.Length() < 1) {
      return ThrowException(Exception::Error(
            String::New("Expecting Callback Function argument")));
    }
    
    tr_req->conn = conn;
    tr_req->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    tr_req->commit = true;
    
    eio_custom(EIO_TransactionRequest, EIO_PRI_DEFAULT, EIO_After_TransactionRequest, tr_req);
    
    ev_ref(EV_DEFAULT_UC);
    conn->Ref();
    
    return Undefined();
  }
  
  static Handle<Value>
  Rollback (const Arguments& args)
  {
    HandleScope scope;
    Connection *conn = ObjectWrap::Unwrap<Connection>(args.This());
    
    struct transaction_request *tr_req =
         (struct transaction_request *)calloc(1, sizeof(struct transaction_request));

    if (!tr_req) {
      V8::LowMemoryNotification();
      return ThrowException(Exception::Error(
            String::New("Could not allocate memory.")));
    }
    
    if (args.Length() < 1) {
      return ThrowException(Exception::Error(
            String::New("Expecting Callback Function argument")));
    }
    
    tr_req->conn = conn;
    tr_req->callback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    tr_req->commit = false;
    
    eio_custom(EIO_TransactionRequest, EIO_PRI_DEFAULT, EIO_After_TransactionRequest, tr_req);
    
    ev_ref(EV_DEFAULT_UC);
    conn->Ref();
    
    return Undefined();
  } 
  
  static Handle<Value>
  QuerySync(const Arguments& args)
  {
    Connection *connection = ObjectWrap::Unwrap<Connection>(args.This());
    
    HandleScope scope;
    
    if (args.Length() < 1 || !args[0]->IsString()){
       return ThrowException(Exception::Error(
            String::New("Expecting a string query argument.")));
    }
    
    String::Utf8Value Query(args[0]->ToString());
    
    XSQLDA *sqlda = NULL;
    isc_stmt_handle stmt = NULL;
    bool r = connection->process_statement(&sqlda,*Query, &stmt);
    if(!r) {
      return ThrowException(Exception::Error(
            String::Concat(String::New("In querySync - "),ERR_MSG(connection, Connection))));
    }
    
    

    Local<Value> argv[2];

    argv[0] = External::New(sqlda);
    argv[1] = External::New(&stmt);
    Persistent<Object> js_result(FBResult::constructor_template->
                                     GetFunction()->NewInstance(2, argv));

    return scope.Close(js_result); 
        
  }
  
  struct query_request {
     Persistent<Function> callback;
     Connection *conn;
     String::Utf8Value *Query;
     XSQLDA *sqlda;
     isc_stmt_handle stmt;
  };
  
  static int EIO_After_Query(eio_req *req)
  {
    ev_unref(EV_DEFAULT_UC);
    HandleScope scope;
    struct query_request *q_req = (struct query_request *)(req->data);

    Local<Value> argv[2];
    
    if (!req->result) {
       argv[0] = Exception::Error(
            String::Concat(String::New("While query - "),ERR_MSG(q_req->conn, Connection)));
       argv[1] = Local<Value>::New(Null());        
    }
    else{
     argv[0] = External::New(q_req->sqlda);
     argv[1] = External::New(&q_req->stmt);
     Persistent<Object> js_result(FBResult::constructor_template->
                                     GetFunction()->NewInstance(2, argv));
     argv[1] = Local<Value>::New(scope.Close(js_result));    
     argv[0] = Local<Value>::New(Null());
    }

    TryCatch try_catch;

    q_req->callback->Call(Context::GetCurrent()->Global(), 2, argv);

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }

    q_req->callback.Dispose();
    q_req->conn->Unref();
    free(q_req);

    return 0;
    
  }
    
  static int EIO_Query(eio_req *req)
  {
    struct query_request *q_req = (struct query_request *)(req->data);
    
    req->result = q_req->conn->process_statement(&q_req->sqlda,**(q_req->Query), &q_req->stmt);
    delete q_req->Query;
    return 0;
  }
  
  static Handle<Value>
  Query(const Arguments& args)
  {
    HandleScope scope;
    Connection *conn = ObjectWrap::Unwrap<Connection>(args.This());
    
    struct query_request *q_req =
         (struct query_request *)calloc(1, sizeof(struct query_request));

    if (!q_req) {
      V8::LowMemoryNotification();
      return ThrowException(Exception::Error(
            String::New("Could not allocate memory.")));
    }
    
    if (args.Length() < 2 || !args[0]->IsString() ||
          !args[1]->IsFunction()) {
      return ThrowException(Exception::Error(
            String::New("Expecting 1 string argument and 1 Function")));
    }
    
    q_req->conn = conn;
    q_req->callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
    q_req->Query = new String::Utf8Value(args[0]->ToString());
    q_req->sqlda = NULL;
    q_req->stmt = 0;
    
    eio_custom(EIO_Query, EIO_PRI_DEFAULT, EIO_After_Query, q_req);
    
    ev_ref(EV_DEFAULT_UC);
    conn->Ref();
    
    return Undefined();
    
  }
  
  static Handle<Value>
  addEvent(const Arguments& args)
  {
    
    
    HandleScope scope;
    Connection *conn = ObjectWrap::Unwrap<Connection>(args.This());
    
    if (args.Length() < 1 || !args[0]->IsString()) {
      return ThrowException(Exception::Error(
            String::New("Expecting 1 string argument")));
    }
    
    String::Utf8Value Event(args[0]->ToString());
    
    if(!event_block::FindBlock(conn->fb_events, *Event)){
        
        event_block* eb;
        
        return event_block::RegEvent(&(conn->fb_events), *Event, conn, &conn->db);
        
    }
    
    return Undefined();
  
  }
  
  static Handle<Value>
  deleteEvent(const Arguments& args)
  {
  
    HandleScope scope;
    Connection *conn = ObjectWrap::Unwrap<Connection>(args.This());
    
    if (args.Length() < 1 || !args[0]->IsString()) {
      return ThrowException(Exception::Error(
            String::New("Expecting 1 string argument")));
    }
    
    String::Utf8Value Event(args[0]->ToString());
    
    return event_block::RemoveEvent(&(conn->fb_events), *Event);

  }
  
  static time_t 
  get_gmt_delta()
  {
              int h1 = 0, h2 = 0, m1 = 0, m2 = 0, d1 = 0, d2 = 0;
              time_t rawtime;
              struct tm timeinfo;
              time(&rawtime);
              if (!localtime_r(&rawtime, &timeinfo)) {
                  return 0;
              }
              h1 = timeinfo.tm_hour - (timeinfo.tm_isdst > 0 ? 1 : 0);
              m1 = timeinfo.tm_min;
              d1 = timeinfo.tm_mday;
              if (!gmtime_r(&rawtime, &timeinfo)) {
                  return 0;
              }
              h2 = timeinfo.tm_hour;
              m2 = timeinfo.tm_min;
              d2 = timeinfo.tm_mday;
              return (((h1 - h2)%24)*60 + (m1-m2))*60;
               
  }
  
    
   Connection () : EventEmitter () 
  {
    db = NULL;
    trans = NULL;
    fb_events = NULL;
    in_async = false;
    connected = false;
  }

  ~Connection ()
  {
    if(db!=NULL) Close();
    assert(db == NULL);
  }
 
 
 private:
    void Event (int revents)
  {

  }

  ISC_STATUS_ARRAY status;
  isc_db_handle db;
  event_block* fb_events;
  bool connected; 
  isc_tr_handle trans;
  bool in_async;
  char err_message[MAX_ERR_MSG_LEN];
  

};

extern "C" void
init (Handle<Object> target) 
{
  HandleScope scope;
  FBResult::Initialize(target);
  Connection::Initialize(target);
}

/*!
 * Copyright by Denys Khanzhiyev aka xdenser
 *
 * See license text in LICENSE file
 */

#include "./fb-bindings-fbresult.h"

 

 Persistent<FunctionTemplate> FBResult::constructor_template;
 
 const double FBResult::dscales[19] =  { 1, 1E1, 1E2, 1E3, 1E4, 1E5, 1E6, 1E7, 1E8, 1E9, 1E10,
                    1E11, 1E12, 1E13, 1E14, 1E15, 1E16, 1E17, 1E18 };
 void
   FBResult::Initialize (v8::Handle<v8::Object> target)
  {
    
    NanScope();
    
    Local<FunctionTemplate> t =  NanNew<FunctionTemplate>(FBResult::New);

    t->Inherit(NanNew(FBEventEmitter::constructor_template));
    
    NanAssignPersistent(constructor_template,t);

    t->SetClassName(NanNew("FBResult"));

    Local<ObjectTemplate> instance_template = t->InstanceTemplate();
        
    NODE_SET_PROTOTYPE_METHOD(t, "fetchSync", FetchSync);
    NODE_SET_PROTOTYPE_METHOD(t, "fetch", Fetch);


    instance_template->SetInternalFieldCount(1);
    
    Local<v8::ObjectTemplate> instance_t = t->InstanceTemplate();
    instance_t->SetAccessor(NanNew("inAsyncCall"),InAsyncGetter);
    
    target->Set(NanNew("FBResult"), t->GetFunction());
  }
  
  
NAN_METHOD(FBResult::New)
  {
  // XSQLDA **asqldap, isc_stmt_handle *astmtp
    NanScope();
    
    REQ_EXT_ARG(0, js_sqldap);
    REQ_EXT_ARG(1, js_stmtp);
    REQ_EXT_ARG(2, js_connection);
    
    XSQLDA *asqldap;
    asqldap = static_cast<XSQLDA *>(js_sqldap->Value());
    isc_stmt_handle *astmtp;
    astmtp = static_cast<isc_stmt_handle *>(js_stmtp->Value()); 
    Connection  *conn;
    conn = static_cast<Connection *>(js_connection->Value()); 
    
    FBResult *res = new FBResult(asqldap, astmtp, conn);
    res->Wrap(args.This());

    NanReturnValue(args.This());
  }
  
  bool FBResult::prepare_sqlda(XSQLDA *sqlda)
  {
    int i;
    XSQLVAR* var;
    sqlda->sqln = sqlda->sqld;
    for(i = 0, var = sqlda->sqlvar; i < sqlda->sqld;i++,var++)
    {
      switch(var->sqltype & ~1)
      {
        case SQL_ARRAY:
        case SQL_BLOB:      var->sqldata = (char*) new ISC_QUAD;
                            memset(var->sqldata, 0, sizeof(ISC_QUAD));
                            break;
        case SQL_TIMESTAMP: var->sqldata = (char*) new ISC_TIMESTAMP;
                            memset(var->sqldata, 0, sizeof(ISC_TIMESTAMP));
                            break;
        case SQL_TYPE_TIME: var->sqldata = (char*) new ISC_TIME;
                            memset(var->sqldata, 0, sizeof(ISC_TIME));
                            break;                                 
        case SQL_TYPE_DATE: var->sqldata = (char*) new ISC_DATE;
                            memset(var->sqldata, 0, sizeof(ISC_DATE));
                            break;                              
        case SQL_TEXT:      var->sqldata = new char[var->sqllen + 1];
                            memset(var->sqldata, ' ', var->sqllen);
                            //memset(var->sqldata, 0, var->sqllen);
                            var->sqldata[var->sqllen] = '\0';
                            break;
        case SQL_VARYING:   var->sqldata = new char[var->sqllen + 3];
                            memset(var->sqldata, 0, 2);
                            memset(var->sqldata + 2, ' ', var->sqllen);
                            var->sqldata[var->sqllen + 2] = '\0';
                            break;
        case SQL_SHORT:     var->sqldata = (char *) new int16_t(0); break;
        case SQL_LONG:      var->sqldata = (char *) new int32_t(0); break;
        case SQL_INT64:     var->sqldata = (char *) new int64_t(0); break;
        case SQL_FLOAT:     var->sqldata = (char *) new float(0.0); break;
        case SQL_DOUBLE:    var->sqldata = (char *) new double(0.0); break;
        default:            return false;                     
      }
      if(var->sqltype & 1){
       var->sqlind = new short(-1);
      }
    }
    return true;
  }
  
  void FBResult::clean_sqlda(XSQLDA *sqlda)
  {
    int i;
    XSQLVAR* var;
    for(i = 0, var= sqlda->sqlvar; i < sqlda->sqld;i++,var++)
    {
      switch(var->sqltype & ~1)
      {
        case SQL_ARRAY:
        case SQL_BLOB:      delete (ISC_QUAD*) var->sqldata; break;
        case SQL_TIMESTAMP: delete (ISC_TIMESTAMP*) var->sqldata; break;
        case SQL_TYPE_TIME: delete (ISC_TIME*) var->sqldata; break;                                 
        case SQL_TYPE_DATE: delete (ISC_DATE*) var->sqldata; break;                              
        case SQL_TEXT:      
        case SQL_VARYING:   delete [] var->sqldata; break;
        case SQL_SHORT:     delete (int16_t *) var->sqldata; break;
        case SQL_LONG:      delete (int32_t *) var->sqldata; break;
        case SQL_INT64:     delete (int64_t *) var->sqldata; break;
        case SQL_FLOAT:     delete (float *) var->sqldata; break;
        case SQL_DOUBLE:    delete (double *) var->sqldata; break;
        default:            return;                     
      }
      if(var->sqlind != 0) delete var->sqlind;
    }
  }     
/*   
  double get_js_time(struct tm* times){
     double res = 0;
     
  }
*/
bool FBResult::clone_sqlda(XSQLDA *src_sqlda,XSQLDA **dest_sqlda)
  {
     int size = XSQLDA_LENGTH (src_sqlda->sqln);
     XSQLDA *out_sqlda = (XSQLDA *) malloc(size);
     *dest_sqlda = out_sqlda;
     if(!out_sqlda) return false;
     memcpy(out_sqlda,src_sqlda,size); 
     return prepare_sqlda(out_sqlda);
  }
  
char* errmsg1f(char* buf,const char* msg, int arg)
{
  sprintf(buf,msg,arg);
  return buf;
}  

char* errmsg2f(char* buf,const char* msg, int arg1, int arg2)
{
  sprintf(buf,msg,arg1,arg2);
  return buf;
}  

void get_date(struct tm* times, Local<Object> js_date, int* msp)
{
  NanScope();
  Local<Value> val;
  
  val = Local<Function>::Cast(js_date->Get( NanNew("getFullYear") ))->Call(js_date,0,NULL);
  times->tm_year = (int) (val->Int32Value()) - 1900;
  
  val = Local<Function>::Cast(js_date->Get( NanNew("getMonth") ))->Call(js_date,0,NULL);
  times->tm_mon = val->Int32Value();

  val = Local<Function>::Cast(js_date->Get( NanNew("getDate") ))->Call(js_date,0,NULL);
  times->tm_mday = val->Int32Value();

  val = Local<Function>::Cast(js_date->Get( NanNew("getHours") ))->Call(js_date,0,NULL);
  times->tm_hour = val->Int32Value();
  
  val = Local<Function>::Cast(js_date->Get( NanNew("getMinutes") ))->Call(js_date,0,NULL);
  times->tm_min = val->Int32Value();

  val = Local<Function>::Cast(js_date->Get( NanNew("getSeconds") ))->Call(js_date,0,NULL);
  times->tm_sec = val->Int32Value();
  
  val = Local<Function>::Cast(js_date->Get( NanNew("getMilliseconds") ))->Call(js_date,0,NULL);
  *msp = val->Int32Value();
  
}

  
void FBResult::set_params(XSQLDA *sqlda, _NAN_METHOD_ARGS_TYPE args)
  {
    NanScope();
    int i;
    XSQLVAR* var;
    PARAMVARY   *vary2;
    FBblob *blob; 
    Local<Object> obj;
    char errm[512];
   // double date_num;
    double double_val;
    struct tm  times;
    int m_sec;
    int16_t s_len;
    int64_t int_val;
//    char *txt;
    
    if( sqlda->sqld >  args.Length() ) {
    	 NanThrowError(errmsg2f(errm,"Expecting %d arguments, but only %d provided.",(int)sqlda->sqld,(int)args.Length()));
    	 return ;
    }
    for(i = 0, var= sqlda->sqlvar; i < sqlda->sqld;i++,var++)
    {
      
      if(args[i]->IsNull() && (var->sqltype & 1)){
       *var->sqlind = -1;
       continue;
      }
      
      switch(var->sqltype & ~1)
      { 
        case SQL_ARRAY:
        case SQL_BLOB:      
                            if(!FBblob::HasInstance(args[i])){
                            	NanThrowError(errmsg1f(errm,"Expecting FBblob as argument #%d.",i+1));
                            	return ;
                            }
                            obj = args[i]->ToObject();  
                            blob = ObjectWrap::Unwrap<FBblob>(obj);  
                            
                            //memcpy(dest,src,size); 
                            blob->getId((ISC_QUAD*)var->sqldata);
                            // printf("setting blob param %d,%d\n",((ISC_QUAD*)var->sqldata)->gds_quad_high,((ISC_QUAD*)var->sqldata)->gds_quad_low);
                            break;
                            
        case SQL_TIMESTAMP: 
                            if(!args[i]->IsDate()) {
                            	NanThrowError(errmsg1f(errm,"Expecting Date as argument #%d.",i+1));
                            	return ;
                            }
                                                
                            get_date( &times, args[i]->ToObject(), &m_sec);                            
                            isc_encode_timestamp(&times, (ISC_TIMESTAMP *)var->sqldata);
                            ((ISC_TIMESTAMP *)var->sqldata)->timestamp_time = ((ISC_TIMESTAMP *)var->sqldata)->timestamp_time + m_sec*10;
                            break;          
        case SQL_TYPE_TIME:  
                            if(!args[i]->IsDate()){
                            	NanThrowError(errmsg1f(errm,"Expecting Date as argument #%d.",i+1));
                            	return ;
                            }
                            get_date( &times, args[i]->ToObject(), &m_sec);                            
                            isc_encode_sql_time(&times, (ISC_TIME *)var->sqldata);
                            *((ISC_TIME *)var->sqldata) = *((ISC_TIME *)var->sqldata) + m_sec*10;
                            break;                                          
        case SQL_TYPE_DATE:  
                            if(!args[i]->IsDate()){
                            	NanThrowError(errmsg1f(errm,"Expecting Date as argument #%d.",i+1));
                            	return ;
                            }
                            get_date( &times, args[i]->ToObject(), &m_sec);                            
                            isc_encode_sql_date(&times, (ISC_DATE *)var->sqldata);
                            break;                               
        case SQL_TEXT:      
                            if(!args[i]->IsString()){
                            	NanThrowError(errmsg1f(errm,"Expecting String as argument #%d.",i+1));
                            	return ;
                            }
                            {                           
                            String::Utf8Value txt(args[i]->ToString());  
                            s_len = strlen(*txt);
                            if(s_len > var->sqllen) s_len = var->sqllen;                             
                            strncpy(var->sqldata, *txt, s_len);
                            while(s_len < var->sqllen) var->sqldata[s_len++] = ' ';
                            }
			    break;

	case SQL_VARYING:		    
	                    
	                    if(!args[i]->IsString()) {
	                    	NanThrowError(errmsg1f(errm,"Expecting String as  argument #%d.",i+1));
	                    	return ;
	                    }
                            {                           
                            String::Utf8Value txt(args[i]->ToString());  
                            
                            s_len = strlen(*txt);
                            if(s_len > var->sqllen) s_len = var->sqllen;
                            vary2 = (PARAMVARY*) var->sqldata;
                            vary2->vary_length = s_len;                            
                            strncpy((char*) vary2->vary_string, *txt, s_len);
                            }
			    break;
        case SQL_SHORT:	
                            int_val = args[i]->IntegerValue();
                            *(int16_t *) var->sqldata = (int16_t) int_val;
                            break;
        case SQL_LONG:      
                            int_val = args[i]->IntegerValue();
                            *(int32_t *) var->sqldata = (int32_t) int_val;
                            break;                     
        case SQL_INT64:                        
                            if(args[i]->IsNumber())
                            {
                              double multiplier = FBResult::dscales[-var->sqlscale];
                              double_val = args[i]->NumberValue();
                              *(int64_t *) var->sqldata = (int64_t) floor(double_val * multiplier + 0.5);
                            }
                            else 
                            {
                              int_val = args[i]->IntegerValue();
                              *(int64_t *) var->sqldata = int_val;
                            }  
                            break;                      
        case SQL_FLOAT:     
                            double_val = args[i]->NumberValue();
                            *(float *) var->sqldata = (float) double_val;
                            break;
        case SQL_DOUBLE:                    
                            double_val = args[i]->NumberValue();  
                            if( var->sqlscale != 0 )
                            {
                              double multiplier = FBResult::dscales[-var->sqlscale];
                              *(double *) var->sqldata = floor(double_val * multiplier + 0.5) / multiplier;
                            } 
                            else  *(double *) var->sqldata = double_val;
                            break;
      }
      if(var->sqltype & 1) *var->sqlind = 0;
    }
    
  }

short getCharsetSize(XSQLVAR *var){
	switch(var->sqlsubtype & 0xFF){
	 case 0: case 1: case 2: case 10: case 11: case 12: case 13: case 14:
	 case 19: case 21: case 22: case 39: case 45: case 46: case 47:
	 case 50: case 51: case 52: case 53: case 54: case 55: case 58:  return 1;
	 
	 case 5: case 6: case 8: case 44: case 56: case 57: case 64: return 2;
	 
	 case 3: return 3;
	 
	 case 4: case 59: return 4; 
	}
	return 0;
}
  
Local<Value> 
  FBResult::GetFieldValue(XSQLVAR *var, Connection* conn)
  {
    short       dtype;  
    PARAMVARY   *vary2;
  //  short       len; 
    struct tm   times;
    ISC_QUAD    bid;
    //time_t      rawtime;
  //  double      time_val;
   // int 	    days;
    short		bpc, chars; // bytes per char 
    
    
    NanEscapableScope();
    
    Local<Function> con;
    Local<Value> argv[7];
    
    Local<Object> js_date, js_obj;
    Local<Value> js_field = NanNull();
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
            	bpc = getCharsetSize(var);
            	chars =  var->sqllen/(bpc != 0 ? bpc : 1);
                js_field = NanNew<String>(var->sqldata,var->sqllen );
                if(Local<String>::Cast(js_field)->Length() > chars )
                {
                	js_obj = js_field->ToObject(); 
                	argv[0] = NanNew<Integer>(0);
                	argv[1] = NanNew<Integer>(chars);
                	js_field = NanMakeCallback(js_obj,"slice",2,argv);
                }
                
               //  printf(" char lengh %d/%d, %d, 1 %hx, 2 %hx, 3 %hx, 4 %hx \n",var->sqllen,Local<String>::Cast(js_field)->Length(), var->sqlsubtype, var->sqldata[0],var->sqldata[1],var->sqldata[2],var->sqldata[3]);
                //  js_field = String::New(var->sqldata);
                break;

            case SQL_VARYING:
                vary2 = (PARAMVARY*) var->sqldata;
                vary2->vary_string[vary2->vary_length] = '\0';
                js_field = NanNew<String>((const char*)(vary2->vary_string));
                break;

            case SQL_SHORT:
            case SQL_LONG:
	    case SQL_INT64:
		{
		ISC_INT64	val = 0;
	//	short		field_width;
		short		dscale;
		switch (dtype)
		    {
		    case SQL_SHORT:
			val = (ISC_INT64) *(short *) var->sqldata;
		//	field_width = 6;
			break;
		    case SQL_LONG:
			val = (ISC_INT64) *(int *) var->sqldata;
		//	field_width = 11;
			break;
		    case SQL_INT64:
			val = (ISC_INT64) *(ISC_INT64 *) var->sqldata;
		//	field_width = 21;
			break;
		    }
		ISC_INT64	tens;    
		short	i;
		dscale = var->sqlscale;
		
		if (dscale < 0)
		    {
		    tens = 1;
		    for (i = 0; i > dscale; i--) tens *= 10;
                    js_field = NanNew<Number>((double) val / tens); 
                    
		    }
		else if (dscale)
		      {
		       tens = 1;
		       for (i = 0; i < dscale; i++) tens *= 10;
		       js_field = NanNew<Number>(double(val * tens));
	              }		    
		else
		      js_field = NanNew<Number>((double) val);
		}
                break;

            case SQL_FLOAT:
                js_field = NanNew<Number>(double( *(float *) (var->sqldata) ));  
                break;

            case SQL_DOUBLE:
                js_field = NanNew<Number>(*(double *) (var->sqldata));
                break;

            case SQL_TIMESTAMP: 
	            isc_decode_timestamp((ISC_TIMESTAMP *)var->sqldata, &times);
	            js_date = NanNew<Date>(0);
	            argv[0] = NanNew<Integer>(times.tm_year+1900);
	            NanMakeCallback(js_date,"setFullYear",1,argv);
	            argv[0] = NanNew<Integer>(times.tm_mon);
	            NanMakeCallback(js_date,"setMonth",1,argv);
	            argv[0] = NanNew<Integer>(times.tm_mday);
	            NanMakeCallback(js_date,"setDate",1,argv);
	            argv[0] = NanNew<Integer>(times.tm_hour);
	            NanMakeCallback(js_date,"setHours",1,argv);
	            argv[0] = NanNew<Integer>(times.tm_min);
	            NanMakeCallback(js_date,"setMinutes",1,argv);
	            argv[0] = NanNew<Integer>(times.tm_sec);
	            NanMakeCallback(js_date,"setSeconds",1,argv);
	            argv[0] = NanNew<Integer>(  (( ((ISC_TIMESTAMP *)var->sqldata)->timestamp_time) % 10000) / 10);
	            NanMakeCallback(js_date,"setMilliseconds",1,argv);
	            
	            js_field = js_date;
	            break;
	            
            case SQL_TYPE_DATE:    
	            	          
	            isc_decode_sql_date((ISC_DATE *)var->sqldata, &times);
	            
	            // days from 1 jan 1858 	            
	            //days = * (int *) var->sqldata ; 
	            
	            
	            //time_val = (static_cast<double>(days) - 40587) * 86400 * 1000;
	            js_date = NanNew<Date>(0);
	           
	            
	            argv[0] = NanNew<Integer>(times.tm_year+1900);
	            NanMakeCallback(js_date,"setFullYear",1,argv);
	            argv[0] = NanNew<Integer>(times.tm_mon);
	            NanMakeCallback(js_date,"setMonth",1,argv);
	            argv[0] = NanNew<Integer>(times.tm_mday);
	            NanMakeCallback(js_date,"setDate",1,argv);
	            argv[0] = NanNew<Integer>(0);
	            NanMakeCallback(js_date,"setHours",1,argv);
	            NanMakeCallback(js_date,"setMinutes",1,argv);
	            NanMakeCallback(js_date,"setSeconds",1,argv);
	            NanMakeCallback(js_date,"setMilliseconds",1,argv);
	            
	            //js_field = con->Call(js_date, 3, argv);
	            /*js_date = Object::New();
	            js_date->Set(String::New("year"),
                             Integer::New(times.tm_year+1900));
                    js_date->Set(String::New("month"),
                             Integer::New(times.tm_mon+1));
                    js_date->Set(String::New("day"),
                             Integer::New(times.tm_mday));
                             
	            
	            js_date->Set(String::New("date"),
                             Date::New(time_val));*/
	            js_field = js_date;
	            break;
	            
           case SQL_TYPE_TIME:    
	            isc_decode_sql_time((ISC_TIME *)var->sqldata, &times);
	            
	            js_date = NanNew<Date>(0);
	      	    argv[0] = NanNew<Integer>(times.tm_year+1900);
	      	    NanMakeCallback(js_date,"setFullYear",1,argv);
	      	    argv[0] = NanNew<Integer>(times.tm_mon);
	      	    NanMakeCallback(js_date,"setMonth",1,argv);
	      	    argv[0] = NanNew<Integer>(times.tm_mday);
	      	    NanMakeCallback(js_date,"setDate",1,argv);
	      	    argv[0] = NanNew<Integer>(times.tm_hour);
	      	    NanMakeCallback(js_date,"setHours",1,argv);
	      	    argv[0] = NanNew<Integer>(times.tm_min);
	      	    NanMakeCallback(js_date,"setMinutes",1,argv);
	      	    argv[0] = NanNew<Integer>(times.tm_sec);
	      	    NanMakeCallback(js_date,"setSeconds",1,argv);
	      	    argv[0] = NanNew<Integer>(  (( ((ISC_TIMESTAMP *)var->sqldata)->timestamp_time) % 10000) / 10);
	      	    NanMakeCallback(js_date,"setMilliseconds",1,argv);
	            js_field = js_date;
	            break;            

            case SQL_BLOB:
            case SQL_ARRAY:
                /* Print the blob id on blobs or arrays */
                bid = *(ISC_QUAD *) var->sqldata;
                
                argv[0] = NanNew<External>(&bid);
		        argv[1] = NanNew<External>(conn);
                Local<Object> js_blob = NanNew(FBblob::constructor_template)->GetFunction()->NewInstance(2, argv);

                js_field = js_blob;
                break;

        }

    }
    
    return NanEscapeScope(js_field);
  }
  
Local<Object> 
  FBResult::getCurrentRow(bool asObject)
  {
    short  i, num_cols;
   
    NanEscapableScope();
    Local<Object> js_result_row;   
    Local<Value> js_field;
    
    if(!sqldap) num_cols = 0;
    else num_cols = sqldap->sqld;
 
    
    if(asObject)
            js_result_row = NanNew<Object>();
         else 
            js_result_row = NanNew<Array>();
        
    for (i = 0; i < num_cols; i++)
    {
            js_field = FBResult::GetFieldValue((XSQLVAR *) &sqldap->sqlvar[i], connection);
            if(asObject)
            { 
            	js_result_row->Set(NanNew<String>(sqldap->sqlvar[i].aliasname,sqldap->sqlvar[i].aliasname_length), js_field);
            	//js_result_row->Set(String::New(sqldap->sqlvar[i].sqlname,sqldap->sqlvar[i]), js_field);
            }
            else
            js_result_row->Set(NanNew<Integer>(i), js_field);
    }    
    
    return NanEscapeScope(js_result_row);
  }  

NAN_METHOD(FBResult::FetchSync)
  {
  
    NanScope();
    
    FBResult *fb_res = ObjectWrap::Unwrap<FBResult>(args.This());
    
    int            fetch_stat;
  //  short 	   i, num_cols;	
    XSQLDA         *sqlda;
    
    uint32_t       j = 0;
    sqlda = fb_res->sqldap;
    if(!sqlda){ NanReturnUndefined();}
   // num_cols = sqlda->sqld;
    
    Local<Value> js_result = NanNull();
    
    if (args.Length() < 2){
       return NanThrowError("Expecting 2 arguments");
    }
    
    int rowCount = -1;
    if(args[0]->IsInt32()){
       rowCount = args[0]->IntegerValue();
    }
    else if(! (args[0]->IsString() && args[0]->Equals(NanNew("all")))){
       return NanThrowError("Expecting integer or string as first argument");
    };
    if(rowCount<=0) rowCount = -1;
    
    bool rowAsObject = false;
    if(args[1]->IsBoolean()){
         rowAsObject = args[1]->BooleanValue();
    }else if(args[1]->IsString() && args[1]->Equals(NanNew("array"))){
         rowAsObject = false;
    }else if(args[1]->IsString() && args[1]->Equals(NanNew("object"))){
         rowAsObject = true;
    } else{
     return NanThrowError("Expecting bool or string('array'|'object') as second argument");
    };   
    
    Local<Value> js_field;
    Local<Object> js_result_row;   
        
    Local<Array> res = NanNew<Array>(); 
    while (((fetch_stat = isc_dsql_fetch(fb_res->status, &fb_res->stmt, SQL_DIALECT_V6, sqlda)) == 0)&&((rowCount==-1)||(rowCount>0)))
    {
        js_result_row = fb_res->getCurrentRow(rowAsObject);
    /*     if(rowAsObject)
            js_result_row = Object::New();
         else 
            js_result_row = Array::New();
        
        for (i = 0; i < num_cols; i++)
        {
            js_field = FBResult::GetFieldValue((XSQLVAR *) &sqlda->sqlvar[i], fb_res->connection);
            if(rowAsObject)
            { 
              js_result_row->Set(String::New(sqlda->sqlvar[i].sqlname), js_field);
            }
            else
            js_result_row->Set(Integer::NewFromUnsigned(i), js_field);
        }*/
        res->Set(NanNew<Uint32>(j++), js_result_row);
        if(rowCount>0) rowCount--;
    }
    
    if ((fetch_stat != 100L) && fetch_stat) 
          return NanThrowError(
            String::Concat(NanNew("While FetchSync - "),ERR_MSG(fb_res,FBResult)));

    
//    if(j==1) js_result = res->Get(0);
//    else 
    js_result = res;
    
    NanReturnValue(js_result);

  }

void FBResult::EIO_After_Fetch(uv_work_t *req)
  {
   // uv_unref(uv_default_loop());
    NanScope();
    struct fetch_request *f_req = (struct fetch_request *)(req->data);
	delete req;
   // short i, num_cols;
   // num_cols = f_req->res->sqldap->sqld;
    
    Local<Value> js_field;
    Local<Object> js_result_row;   
    Local<Value> argv[3];
    int argc = 0;
    
    if(f_req->result)
    {
        if(f_req->rowCount>0) f_req->rowCount--;  
        
        js_result_row = f_req->res->getCurrentRow(f_req->rowAsObject);
        /*
	if(f_req->rowAsObject)
    	    js_result_row = Object::New();
	else 
    	    js_result_row = Array::New();
    
	for (i = 0; i < num_cols; i++)
	{
    	    js_field = FBResult::GetFieldValue((XSQLVAR *) &f_req->res->sqldap->sqlvar[i], f_req->res->connection);
    	    if(f_req->rowAsObject)
    	    { 
        	js_result_row->Set(String::New(f_req->res->sqldap->sqlvar[i].sqlname), js_field);
    	    }
    	    else
        	js_result_row->Set(Integer::NewFromUnsigned(i), js_field);
        }
        */
        if(f_req->rowCallback){
    	    argv[0] = js_result_row;
        
	    TryCatch try_catch;
	    
	    Local<Value> ret = NanNew(f_req->rowCallback->Call(1, argv));

	    if (try_catch.HasCaught()) {
    		node::FatalException(try_catch);
	    }
	    else
	    if((!ret->IsBoolean() || ret->BooleanValue())&&f_req->rowCount!=0)
	    {
		    
			uv_work_t* req = new uv_work_t();
            req->data = f_req;
            uv_queue_work(uv_default_loop(), req, EIO_Fetch,  (uv_after_work_cb)EIO_After_Fetch);	


        	//uv_ref(uv_default_loop());
        	return;
	    }
	    else
	    {
	      argc = 2;
	      argv[0] = NanNull();
	      argv[1] = NanFalse();
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
          argv[0] = NanError(*NanAsciiString(
            String::Concat(NanNew("While fetching - "),ERR_MSG(f_req->res,FBResult))));
    }
    else
    {
          argc = 2;
          argv[0] = NanNull();
          argv[1] = NanTrue();
    }

   
    TryCatch try_catch;
    f_req->eofCallback->Call(argc, argv);

    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }
    

    f_req->res->stop_async();
    f_req->res->Unref();
    free(f_req);

  } 

void FBResult::EIO_Fetch(uv_work_t *req)
  {
    struct fetch_request *f_req = (struct fetch_request *)(req->data);
    
    f_req->fetchStat = isc_dsql_fetch(f_req->res->status, &f_req->res->stmt, SQL_DIALECT_V6, f_req->res->sqldap);
    
    f_req->result = (f_req->fetchStat == 0);

    return ;
  }
  
NAN_METHOD(FBResult::Fetch)
  {
    NanScope();
    FBResult *res = ObjectWrap::Unwrap<FBResult>(args.This());
    
    struct fetch_request *f_req =
         (struct fetch_request *)calloc(1, sizeof(struct fetch_request));

    if (!f_req) {
      NanLowMemoryNotification();
      return NanThrowError("Could not allocate memory.");
    }
    
    if (args.Length() != 4){
       return NanThrowError("Expecting 4 arguments");
    }
    
    f_req->rowCount = -1;
    if(args[0]->IsInt32()){
       f_req->rowCount = (int) args[0]->IntegerValue();
    }
    else if(! (args[0]->IsString() && args[0]->Equals(NanNew("all")))){
       return NanThrowError("Expecting integer or string as first argument");
    };
    if(f_req->rowCount<=0) f_req->rowCount = -1;
    
    f_req->rowAsObject = false;
    if(args[1]->IsBoolean()){
         f_req->rowAsObject = args[1]->BooleanValue();
    }else if(args[1]->IsString() && args[1]->Equals(NanNew("array"))){
         f_req->rowAsObject = false;
    }else if(args[1]->IsString() && args[1]->Equals(NanNew("object"))){
         f_req->rowAsObject = true;
    } else{
     return NanThrowError("Expecting bool or string('array'|'object') as second argument");
    };
    
/*    if(!args[2]->IsFunction())  f_req->rowCallback = Persistent<Value>::New(Null());
    else  f_req->rowCallback = Persistent<Function>::New(Local<Function>::Cast(args[2]));
*/   
    if(args[2]->IsFunction()) f_req->rowCallback  = new NanCallback(Local<Function>::Cast(args[2]));
    else f_req->rowCallback = NULL; 
    
    if(!args[3]->IsFunction()) {
      return NanThrowError("Expecting Function as fourth argument");
    }

    
    f_req->res = res;
    f_req->eofCallback = new NanCallback(Local<Function>::Cast(args[3]));
    
    res->start_async();

	uv_work_t* req = new uv_work_t();
    req->data = f_req;
    uv_queue_work(uv_default_loop(), req, EIO_Fetch,  (uv_after_work_cb)EIO_After_Fetch);	
    
    //uv_ref(uv_default_loop());
    res->Ref();
    
    NanReturnUndefined();
  }

  
 FBResult::FBResult (XSQLDA *asqldap, isc_stmt_handle *astmtp, Connection *conn) : FBEventEmitter () 
  {
    sqldap = asqldap;
    stmt = *astmtp;
    connection = conn;
   // conn->doref();
  }
  
 FBResult::~FBResult()
  {
   
	//printf("Statement freed\n"); 
   if(sqldap) {
     FBResult::clean_sqlda(sqldap);
     free(sqldap);
     sqldap = NULL;
   }
   
   long SQLCODE;
   if(stmt){
	   isc_dsql_free_statement(status,&stmt,DSQL_drop);
	   if (status[1]){
		    SQLCODE = isc_sqlcode(status);
		    if(SQLCODE!=-901) // print error message if error is not 'Invalid Statement Handle', wich is normal when connection closed before FBresult is freed
		      printf("Error in free statement %s, %d, %ld\n",ErrorMessage(status,err_message,sizeof(this->err_message)),status[1],SQLCODE);
		   /*ThrowException(Exception::Error(
		              String::Concat(String::New("In FBResult::~FBResult, isc_dsql_free_statement - "),ERR_MSG(this, FBStatement))));
		   */           
	   }
	   else stmt = NULL;
	   
   }
   
	   
  // printf("fbresult destructor !\n");
  // connection->dounref();
  }
  

/*!
 * Copyright by Denys Khanzhiyev aka xdenser
 *
 * See license text in LICENSE file
 */
#ifndef SRC_FB_BINDINGS_FBRESULT_H_
#define SRC_FB_BINDINGS_FBRESULT_H_


#define BUILDING_NODE_EXTENSION 1
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctime> 
#include <uv-private/eio.h>
#include "./fb-bindings.h"
#include "./fb-bindings-fbeventemitter.h"
#include "./fb-bindings-blob.h"



class FBResult : public FBEventEmitter {

public:

 static Persistent<FunctionTemplate> constructor_template;
 
 static void
   Initialize (v8::Handle<v8::Object> target);
  
// bool process_result(XSQLDA **sqldap, isc_stmt_handle *stmtp, Local<Array> res);
  
 static bool prepare_sqlda(XSQLDA *sqlda);
 static void clean_sqlda(XSQLDA *sqlda);
 static bool clone_sqlda(XSQLDA *src_sqlda,XSQLDA **dest_sqlda);
 static Handle<Value> set_params(XSQLDA *sqlda, const Arguments& args);
 Local<Object> getCurrentRow(bool asObject);
   
protected:  
 static Handle<Value>
  New (const Arguments& args);
  
    
  static Local<Value> 
  GetFieldValue(XSQLVAR *var, Connection* conn);

  static Handle<Value>
  FetchSync(const Arguments& args);

  struct fetch_request {
     Persistent<Value> rowCallback;
     Persistent<Function> eofCallback;
     FBResult *res;
     int rowCount;
     int fetchStat;
     bool rowAsObject;
	 bool result;
  };
  
  static void EIO_After_Fetch(uv_work_t *req);

  static void EIO_Fetch(uv_work_t *req);
  
  static Handle<Value>
  Fetch(const Arguments& args);
  
  FBResult(XSQLDA *asqldap, isc_stmt_handle *astmtp, Connection *conn); 
  
  virtual ~FBResult();
  
  ISC_STATUS_ARRAY status;
  char err_message[MAX_ERR_MSG_LEN];
  XSQLDA *sqldap;
  isc_stmt_handle stmt;
  Connection *connection;
  
private:
  static const double dscales[19];

};

#endif

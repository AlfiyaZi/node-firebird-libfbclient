/*!
 * Copyright by Denys Khanzhiyev aka xdenser
 *
 * See license text in LICENSE file
 */
#include <stdlib.h>
#include <v8.h> 
#include <node.h>
#include "./fb-bindings-blob.h"

v8::Persistent<v8::FunctionTemplate> FBblob::constructor_template;
char FBblob::err_message[MAX_ERR_MSG_LEN];

void FBblob::Initialize (v8::Handle<v8::Object> target)
  {
    HandleScope scope;
    
    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->Inherit(FBEventEmitter::constructor_template);
        
    constructor_template = Persistent<FunctionTemplate>::New(t);
    constructor_template->Inherit(FBEventEmitter::constructor_template);
    constructor_template->SetClassName(String::NewSymbol("FBblob"));

    Local<ObjectTemplate> instance_template =
        constructor_template->InstanceTemplate();
        
    NODE_SET_PROTOTYPE_METHOD(t, "_readSync", ReadSync);
    NODE_SET_PROTOTYPE_METHOD(t, "_read", Read);
    NODE_SET_PROTOTYPE_METHOD(t, "_openSync", OpenSync);
    NODE_SET_PROTOTYPE_METHOD(t, "_closeSync", CloseSync);


    instance_template->SetInternalFieldCount(1);
    
    Local<v8::ObjectTemplate> instance_t = t->InstanceTemplate();
    instance_t->SetAccessor(String::NewSymbol("inAsyncCall"),InAsyncGetter);
    
    target->Set(String::NewSymbol("FBblob"), t->GetFunction());

  }
  
Handle<Value> FBblob::New(const Arguments& args)
  {
    HandleScope scope;

    ISC_QUAD *quad;
    Connection  *conn = NULL;
    
    REQ_EXT_ARG(0, js_quad);
    quad = static_cast<ISC_QUAD *>(js_quad->Value());
    
    if(args.Length() > 1) {
      REQ_EXT_ARG(1, js_connection);
      conn = static_cast<Connection *>(js_connection->Value()); 
    }
    
    FBblob *res = new FBblob(quad, conn);
    res->Wrap(args.This());

    return args.This();
  }
  
Handle<Value> FBblob::ReadSync(const Arguments& args)
  {
    HandleScope scope;
    
    FBblob *blob = ObjectWrap::Unwrap<FBblob>(args.This());
    
    if (!Buffer::HasInstance(args[0])) {
        return ThrowException(Exception::Error(
                String::New("First argument needs to be a buffer")));
    }
    
    Local<Object> buffer_obj = args[0]->ToObject();
    char *buffer_data = Buffer::Data(buffer_obj);
    size_t buffer_length = Buffer::Length(buffer_obj);
    //printf("buffer len %d\n",buffer_length);
    
    ISC_STATUS_ARRAY status;
    int res = blob->read(status, buffer_data, (unsigned short) buffer_length);
    if(res==-1) {
       return ThrowException(Exception::Error(
         String::Concat(String::New("In FBblob::_readSync - "),ERR_MSG_STAT(status, FBblob))));
    }
    
    return scope.Close(Integer::New(res));
  }
  
int FBblob::EIO_After_Read(eio_req *req)
  {
    ev_unref(EV_DEFAULT_UC);
    HandleScope scope;
    struct read_request *r_req = (struct read_request *)(req->data);
    Local<Value> argv[3];
    int argc;
    
    Buffer *slowBuffer = Buffer::New(r_req->buffer, (size_t) r_req->length);
    Local<Object> global = Context::GetCurrent()->Global();
    Local<Function> bufferConstructor =  Local<Function>::Cast(global->Get(String::New("Buffer")));
    Handle<Value> cArgs[3] = {slowBuffer->handle_, Integer::New(r_req->length), Integer::New(0) };
    
    if(r_req->res!=-1)
    {
      argv[0] = Local<Value>::New(Null());
      argv[1] = bufferConstructor->NewInstance(3,cArgs);
      argv[2] = Integer::New(r_req->res);
      argc = 3;
    }
    else
    {
      argv[0] =  Exception::Error(
            String::Concat(String::New("FBblob::EIO_After_Read - "),ERR_MSG_STAT(r_req->status, FBblob)));
      argc = 1;        
    }  
    
    r_req->callback->Call(global, argc, argv);
    
    r_req->callback.Dispose();
    r_req->blob->stop_async();
    r_req->blob->Unref();
    free(r_req);

    return 0;

    
    return 0;
  }
  
int FBblob::EIO_Read(eio_req *req)
  {
    struct read_request *r_req = (struct read_request *)(req->data);
    r_req->res = r_req->blob->read(r_req->status,r_req->buffer,(unsigned short) r_req->length);
    return 0;
  }

Handle<Value> FBblob::Read(const Arguments& args)
  {
    HandleScope scope;
    FBblob *blob = ObjectWrap::Unwrap<FBblob>(args.This());
    
        
    if (args.Length() != 2){
       return ThrowException(Exception::Error(
            String::New("Expecting 4 arguments")));
    }
    
    if (!Buffer::HasInstance(args[0])) {
        return ThrowException(Exception::Error(
                String::New("First argument needs to be a buffer")));
    }
    
    Local<Object> buffer_obj = args[0]->ToObject();
    char *buffer_data = Buffer::Data(buffer_obj);
    size_t buffer_length = Buffer::Length(buffer_obj);
    
    if(!args[1]->IsFunction()) {
      return ThrowException(Exception::Error(
            String::New("Expecting Function as second argument")));
    }
        
    struct read_request *r_req =
         (struct read_request *)calloc(1, sizeof(struct read_request));

    if (!r_req) {
      V8::LowMemoryNotification();
      return ThrowException(Exception::Error(
            String::New("Could not allocate memory.")));
    }
    
    r_req->blob = blob;
    r_req->callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
    r_req->buffer = buffer_data;
    r_req->length = buffer_length;
    r_req->res = 0;

    blob->start_async();
    eio_custom(EIO_Read, EIO_PRI_DEFAULT, EIO_After_Read, r_req);
    
    ev_ref(EV_DEFAULT_UC);
    blob->Ref();
    
    return Undefined();
  }
  
Handle<Value>
FBblob::OpenSync(const Arguments& args)
  {
    HandleScope scope;
    FBblob *blob = ObjectWrap::Unwrap<FBblob>(args.This());
    
    ISC_STATUS_ARRAY status;
    if(!blob->open(status)){
       return ThrowException(Exception::Error(
         String::Concat(String::New("In FBblob::_openSync - "),ERR_MSG_STAT(status, FBblob))));
    } 
    
    return Undefined();
  }
  
Handle<Value>
FBblob::CloseSync(const Arguments& args)
  { 
    HandleScope scope;
    FBblob *blob = ObjectWrap::Unwrap<FBblob>(args.This());
    
    ISC_STATUS_ARRAY status;
    if(!blob->close(status)){
       return ThrowException(Exception::Error(
         String::Concat(String::New("In FBblob::_closeSync - "),ERR_MSG_STAT(status, FBblob))));
    } 
    
    return Undefined();
  }
  
  
FBblob::FBblob(ISC_QUAD *id, Connection *conn): FBEventEmitter () 
  {
    blob_id = *id; 
    connection = conn;
    handle = NULL;
  }

FBblob::~FBblob()
  {
    if(handle!=0) {
      ISC_STATUS_ARRAY status;
      close(status);
    } 
  }    


bool FBblob::open(ISC_STATUS *status)
  {
    if(isc_open_blob2(status, &(connection->db), &(connection->trans), &handle, &blob_id, 0, NULL)) return false;
    return true; 
  }
  
int FBblob::read(ISC_STATUS *status,char *buf, unsigned short len)
  {
    unsigned short a_read;
    int res = isc_get_segment(status, &handle, &a_read, len, buf);
    if(res == isc_segstr_eof) return 0;
    if(res != isc_segment && status[1] != 0) return -1;
    return (int)a_read;
  }
  
bool FBblob::close(ISC_STATUS *status)
  {
    if( handle == 0 ) return true;
    if(isc_close_blob(status, &handle))
	{
	  handle = 0;
          return false;
        }  
    handle = 0;
    return true;
  }
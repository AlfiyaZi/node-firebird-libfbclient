/*!
 * Copyright by Denys Khanzhiyev aka xdenser
 *
 * See license text in LICENSE file
 */
#ifndef SRC_FB_BINDINGS_FBEVENTEMITTER_H_
#define SRC_FB_BINDINGS_FBEVENTEMITTER_H_

#include <node.h>
#include <v8.h>


using namespace node;
using namespace v8;

static Persistent<String> start_async_symbol;
static Persistent<String> stop_async_symbol;

class FBEventEmitter: public ObjectWrap {
public: 
  static void Initialize(v8::Handle<v8::Object> target);
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  void Emit(Handle<String> event, int argc, Handle<Value> argv[]);

protected:
  void start_async();

  void stop_async();

  FBEventEmitter ();

  static Handle<Value>
  InAsyncGetter(Local<String> property,
                      const AccessorInfo &info);
private:
  bool in_async;
};

#endif

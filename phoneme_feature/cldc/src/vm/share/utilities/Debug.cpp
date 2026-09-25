/*
 *   
 *
 * Copyright  1990-2007 Sun Microsystems, Inc. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 only, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is
 * included at /legal/license.txt).
 * 
 * You should have received a copy of the GNU General Public License
 * version 2 along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 or visit www.sun.com if you need additional
 * information or have any questions.
 */

/*
 * Debug.cpp: VM-internal debugging operations
 *
 * This file provides definitions and functions that are 
 * used internally in the VM to support various
 * assertions and internal consistency checks.  These 
 * extra checks are performed in non-product builds only.
 *
 * The general format for adding assertions / consistency 
 * checks to the VM is:
 *
 *     GUARANTEE(condition, errorMessage);
 *
 * where "condition" is an expression that evaluates to
 * a boolean value.  Examples:
 *
 *     GUARANTEE(instance_size > 0, "bad instance size");
 *     GUARANTEE(length >= 0, "Cannot allocate symbol of negative length");
 *            
 * Important: The definitions in this file are related to 
 * the debugging of the virtual machine itself -- not to 
 * debugging of the Java code that is executed by the 
 * virtual machine.
 */

# include "incls/_precompiled.incl"
# include "incls/_Debug.cpp.incl"

/* Diagnostic: enable the pss() (print-all-thread-stacks) hook even in
 * PRODUCT builds. MIDP always links against the PRODUCT CLDC library
 * (libcldc_vm.a) regardless of MIDP's own debug/release setting, so
 * without this, pss() - triggered by System.getProperty("__debug.only.pss"),
 * see Natives.cpp - is silently compiled out and the DEBUG_TRACE1 key
 * (see midp_msgQueue_md.c on vita) does nothing. */
#ifndef ENABLE_PRODUCT_PRINT_STACK
#define ENABLE_PRODUCT_PRINT_STACK 1
#endif

#ifndef AZZERT
#  ifdef _DEBUG
   // NOTE: don't turn the lines below into a comment -- if you're getting
   // a compile error here, change the settings to define AZZERT
   AZZERT should be defined when _DEBUG is defined.  It is not
   intended to be used for debugging functions that do not slow down
   the system too much and thus can be left in optimized code.  On the
   other hand, the code should not be included in a production
   version.
#  endif // _DEBUG
#endif // AZZERT

#ifdef _DEBUG
#  ifndef AZZERT
     configuration error: AZZERT must be defined in debug version
#  endif // AZZERT
#endif // _DEBUG

#ifdef PRODUCT
#  if -defined _DEBUG || -defined AZZERT
     configuration error: AZZERT et al. must not be defined in PRODUCT version
#  endif
#endif // PRODUCT

#if defined(PRODUCT) || !ENABLE_VERBOSE_ASSERTION
void report_fatal(ErrorMsgTag err) {
  tty->print_cr("Fatal error: %s", ErrorMessage::get(err));
  JVM::exit(-1);
}
#endif

#ifdef AZZERT

StaticBufferChecker::StaticBufferChecker(char *buf, int length, int elem_size)
{
  _buf = buf;
  _length = length;
  _elem_size = elem_size;

  char x = (char)0xab;
  char *p = _buf + (_length * _elem_size);
  int pad_bytes = STATIC_BUFFER_PADDING * _elem_size;

  for (int i=0; i<pad_bytes; i++) {
    p[i] = x;
    x++;
  }
}

StaticBufferChecker::~StaticBufferChecker() {
  char x = (char)0xab;
  char *p = _buf + (_length * _elem_size);
  int pad_bytes = STATIC_BUFFER_PADDING * _elem_size;

  for (int i=0; i<pad_bytes; i++) {
    GUARANTEE(p[i] == x, "static buffer overflow");
    x++;
  }
}
#endif

#ifndef PRODUCT
Oop *DebugHandleMarker::_saved_last_handle     = NULL;
Oop *DebugHandleMarker::_saved_last_raw_handle = NULL;
bool DebugHandleMarker::_is_active             = false;

DebugHandleMarker::DebugHandleMarker() {
#ifdef AZZERT
  if (!_is_active) {
    _saved_last_handle     = _last_handle;
    _saved_last_raw_handle = last_raw_handle;
    _is_active             = true;
    _is_outermost          = true;
  } else {
    _is_outermost          = false;
  }
#endif
}

DebugHandleMarker::~DebugHandleMarker() {
#ifdef AZZERT
  if (_is_outermost) {
    GUARANTEE(_is_active, "sanity");
    _is_active = false;
  }
#endif
}

void DebugHandleMarker::restore() {
#ifdef AZZERT
  if (_is_active) {
    _last_handle    = _saved_last_handle;
    last_raw_handle = _saved_last_raw_handle;

    _saved_last_handle     = NULL;
    _saved_last_raw_handle = NULL;
    _is_active             = false;
  }
#endif
}

void warning(const char* format, ...) {
  tty->print("cldc_vm JVM warning: ");
  va_list ap;
  va_start(ap, format);
  tty->vprint_cr(format, ap);
  va_end(ap);
}

#if ENABLE_VERBOSE_ASSERTION
void report_fatal(const char* file_name, int line_no, ErrorMsgTag err) {
  report_error(true, file_name, line_no, "Internal Error", "Fatal: %s", 
               ErrorMessage::get(err));
  BREAKPOINT;
  JVM::exit(-1);
}

void report_assertion_failure(const char* code_str, const char* file_name,
                              int line_no, const char* message) {
  report_error(true, file_name, line_no,
               "assertion failure",
               "assert(%s, \"%s\")", code_str, message);
  BREAKPOINT;
}
void report_should_not_reach_here(const char* file_name, int line_no) {
  report_error(true, file_name, line_no,
               "Internal Error", "ShouldNotReachHere()");
  BREAKPOINT;
}

void report_unimplemented(const char* file_name, int line_no) {
  report_error(true, file_name, line_no,
               "Internal Error", "Unimplemented()");
  BREAKPOINT;
}

void report_error(bool is_vm_internal_error, const char* file_name, 
                  int line_no, const char* title, const char* format,  ...) {  
  static int error_level = 1;
  // Handle the recursive case  
  switch(error_level++) {
   case 1:  // first time, do nothing
            break; 
   case 2:  // second time print recursive problem 
            tty->print_cr("[error occurred during error reporting]"); 
            return;
   default: // otherwise just say NO.
            JVM::exit(-1);
  }
  
  // Compute the message
  char message[2*1024];
  va_list ap;
  va_start(ap, format);
  jvm_vsprintf(message, format, ap);
  va_end(ap);

  if (is_vm_internal_error) {
    // Print error has happen
    tty->cr();
    tty->print_cr("#");
    tty->print_cr("# VM Error, %s", title);
    tty->print_cr("#");

    char loc_buf[256];
    if (file_name != NULL) {
       int len = jvm_strlen(file_name);
       jvm_strncpy(loc_buf, file_name, 256);
       if (len + 10 < 256) {
         jvm_sprintf(loc_buf + len, ", %d", line_no);
       }
    } else {
      jvm_strcpy(loc_buf, "<unknown>");
    }
    tty->print_cr("# Error ID: %s", loc_buf);
    tty->print_cr("#");
  }

  {
    char* begin = message;
    // print line by line
    for (;;) {
      char* end = (char *) jvm_strchr(begin, '\n');
      if (!end) {
        break;
      }
      *end = '\0';
      tty->print_raw("# ");
      tty->print_raw(begin);
      *end = '\n';
      begin = end + 1;
    }
    // print last line
    if (*begin) {
      tty->print_raw("# ");
      tty->print_raw(begin);
      tty->cr();
    }
    // print end mark
    tty->print_cr("#");
  }

  error_level--;
}
#else
static const char *msg1 = "assertion failure: detailed message unavailable";
static const char *msg2 = "should not reach here: detailed message unavailable";
static const char *msg3 = "unimplemented: detailed message unavailable";
void report_assertion_failure() {
  tty->print_cr(msg1);
  BREAKPOINT;
}
void report_should_not_reach_here() {
  tty->print_cr(msg2);
  BREAKPOINT;
}
void report_unimplemented() {
  tty->print_cr(msg3);
  BREAKPOINT;
}
#endif

void find(int x) {
  OopSlot* p = (OopSlot*)(address_word) x;
  if (ObjectHeap::contains_live(p)) {
    Oop o = ObjectHeap::slow_object_start(p);
    tty->print_cr("0x%p in object 0x%p", x, o.obj());
    o.print();
  } else if (ObjectHeap::contains(p)) {
    tty->print_cr("0x%p inside dead region of ObjectHeap", x);
  } else {
    tty->print_cr("0x%p unknown location", x);
  }
}

void ppv(int x) { 
  bool oldVerbose = Verbose;
  Verbose = true;
  pp(x);
  Verbose = oldVerbose;
}

void pp(int x) {
  DebugHandleMarker debug_handle_marker;

  OopSlot* p = (OopSlot*)(address_word) x;
  Oop::Raw o;

  Oop::disable_on_stack_check();

  if (ObjectHeap::contains_live(p)) {
    o = ObjectHeap::slow_object_start(p);
  } else if (ROM::system_contains((OopDesc*)p)) {
    if (ROM::system_data_contains((OopDesc*)p)) {
      // IMPL_NOTE: slow_start system data object
      o = (OopDesc*)p;
    } else if (ROM::is_valid_text_object((OopDesc*)p)) {
      o = (OopDesc*)p;
    }
  } else if (ROM::in_any_loaded_bundle((OopDesc*)p)) {
    // This is in a bundle, but not the system bundle, so it must be
    // in a binary bundle.

    // IMPL_NOTE: slow_start binary bundle object
    o = (OopDesc*)p;
  }

  if (!o.is_null()) {
    if (o.obj() == (OopDesc*)(address_word) x) {
#if ENABLE_ISOLATES
      // We must switch to the context of the task
      int task_id =  ObjectHeap::owner_task_id(o.obj());
      if( task_id == MAX_TASKS ) {
        task_id = TaskContext::current_task_id();
      }
      TaskGCContext tmp(task_id);
#endif
      o().print();
      if (Verbose && o.is_method()) {
        Method::Raw m = o.obj();
        m().print_value_on(tty);
        m().print_bytecodes(tty);
      }
#if ENABLE_COMPILER
      if (Verbose && o.is_compiled_method()) {
        CompiledMethod::Raw m = o.obj();
        m().print_code_on(tty);
      }
#endif
    } else {
      tty->print_cr("0x%x points inside object 0x%lx + %ld",
                    x, (long)(address_word)o.obj(), x - (long)(address_word)o.obj());
    }
  } else {
    tty->print_cr("0x%x not in object space", x);
  }

  Oop::enable_on_stack_check();
}

static int pps_pointer;
static void pps_helper() {
  pp(pps_pointer);
}

// pps = Print on Primordial Stack. Use this while you get stuck on
// the Java stack
void pps(int x) {
  pps_pointer = x;
  call_on_primordial_stack(pps_helper);
}

void ppx(int x) {
  Oop o = (OopDesc*)(address_word)x;
  o.print();
}

void ppxv(int x) {
  bool oldVerbose = Verbose;
  Verbose = true;
  Oop o = (OopDesc*)(address_word)x;
  o.print();
  Verbose = oldVerbose;
}

#if USE_DEBUG_PRINTING
void pao() { 
  ObjectHeap::print_all_objects(tty);
}

void paof() {
  FileStream stream;
  JvmPathChar file[] = {'h', 'e', 'a', 'p', '.', 't', 'x', 't', 0};
  stream.open(file);
  ObjectHeap::print_all_objects(&stream);
  stream.close();
}

void poh() {
  ObjectHeap::print();
}

void ref(int x) {
  ObjectHeap::check_reach_root((OopDesc*)(address_word)x, NULL, -1);
  ObjectHeap::find((OopDesc*)(address_word)x, false);
}
#endif

void print_trace_do(Thread* thread, void do_oop(OopSlot*)) {
    (void)&do_oop;

    tty->print("[Thread: 0x%x", thread->obj());
    if (thread->obj() == Thread::current()->obj()) {
      tty->print_cr(" *** CURRENT ***]");
      ps();
    } else {
      tty->print_cr("]");
      thread->trace_stack(tty);
    }
}

void new_pss() {
  DebugHandleMarker debug_handle_marker;

  tty->print_cr("[Dumping all threads]");
  tty->print_cr("Current thread = 0x%x", Thread::current()->obj());
  tty->print_cr("");

  Scheduler::iterate(print_trace_do);

  tty->print_cr("[Finished dumping all threads]");
}

// Renamed from pss() and disabled: this used to silently shadow the
// MarkerStream-based pss() below whenever !defined(PRODUCT) - which,
// contrary to this file's own old assumption (see top of file), is
// exactly the CLDC variant MIDP actually links against when built with
// USE_DEBUG=true (the debug variant, libcldc_vm_g.a), i.e. this WAS the
// real active pss() all along, and its tty-based output never reaching
// any observable channel was the actual original mystery. Kept here
// disabled rather than deleted in case tty ever needs revisiting.
void old_unused_pss() {
  DebugHandleMarker debug_handle_marker;

  Thread* head = Universe::global_threadlist();
  Thread* current = Thread::current();
  tty->print_cr("[Dumping all threads]");
  tty->cr();
  Scheduler::print();
  int n = 0;

  tty->cr();
  tty->print_cr("Stack traces:");

  // The things you do to avoid creating a handle...
  for (OopDesc* ptr = head->obj(); ptr != NULL; 
       ptr = ((Thread*)&ptr)->global_next()) {
    Thread* thr = (Thread*)&ptr;
    tty->print("[Thread %d: 0x%x (id=%d)", ++n, thr->obj(), thr->id());
    if (thr->obj() == current->obj()) {
      tty->print_cr(" *** CURRENT ***]");
      ps();
    } else {
      tty->print_cr("]");
      thr->trace_stack(tty);
    }
  }
  tty->print_cr("[Finished dumping all %d threads]", n);

  if (!DisableDeadlockFinder) {
    Scheduler::check_deadlocks();
  }
}



/// Dump a TypeArray that would appear as ClassInfoDesc::_local_interfaces
void print_interfaces(OopDesc* interfaces) {
  DebugHandleMarker debug_handle_marker;
  TypeArray::Raw array = interfaces;
  for (int i=0; i<array().length(); i++) {
    InstanceClass::Raw ic = Universe::class_from_id(array().ushort_at(i));

    if (Verbose) {
      tty->print_cr("interfaces[%d]:", i);
      ic().print_on(tty);
    } else {
      tty->print("interfaces[%d] = ", i);
      ic().print_name_on(tty);
      tty->cr();
    }
  }
}

void __method(OopDesc* o) {
  Method method(o);
  BREAKPOINT;
}

void __ic(OopDesc* o) {
  InstanceClass klass(o);
  BREAKPOINT;
}

void __obj_array(OopDesc* o) {
  ObjArray array(o);
  BREAKPOINT;
}

void __symbol(OopDesc* o) {
  Symbol symbol(o);
  BREAKPOINT;
}

void __rhe(OopDesc* o) {
  ROMizerHashEntry entry(o);
  jvm_printf("%d\n", entry.offset());
  //BREAKPOINT;
}


/// Dump a TypeArray that would appear as ClassInfoDesc::_fields
void print_fields(OopDesc* o) {
  DebugHandleMarker debug_handle_marker;
  TypeArray::Raw fields = o;

  for (int i=0; i<fields().length(); i += 5) {
    tty->print_cr("[%d] %6d // ACCESS",    i + Field::ACCESS_FLAGS_OFFSET,
                        fields().ushort_at(i + Field::ACCESS_FLAGS_OFFSET));
    tty->print_cr("[%d] %6d // NAME",      i + Field::NAME_OFFSET,
                        fields().ushort_at(i + Field::NAME_OFFSET));
    tty->print_cr("[%d] %6d // SIGNATURE", i + Field::SIGNATURE_OFFSET,
                        fields().ushort_at(i + Field::SIGNATURE_OFFSET));
    tty->print_cr("[%d] %6d // INITVAL",   i + Field::INITVAL_OFFSET,
                     fields().ushort_at(i + Field::INITVAL_OFFSET));
    tty->print_cr("[%d] %6d // OFFSET",   i + Field::OFFSET_OFFSET,
                     fields().ushort_at(i + Field::OFFSET_OFFSET));
    tty->cr();
  }
}

#if ENABLE_COMPILER

void ra() {
  DebugHandleMarker debug_handle_marker;
  tty->print_cr("RegisterAllocator");
  RegisterAllocator::print();
}

void vsf() {
  DebugHandleMarker debug_handle_marker;
  tty->print_cr("VirtualStackFrame");
  if (Compiler::is_active()) {
    Compiler::current()->frame()->print();
  } else {
    tty->print_cr("  (compiler not active)");
  }
}

void dis() {
  DebugHandleMarker debug_handle_marker;
  CompiledMethod* compiled_method = 
      Compiler::code_generator()->compiled_method();
  compiled_method->print_name_on(tty);
  compiled_method->print_code_on(tty);
  VirtualStackFrame* frame = Compiler::current()->frame();
  frame->dump_fp_registers(false);
}

void fpu() {
  tty->print_cr("FPUstack");
  if (Compiler::is_active()) {
    Compiler::current()->frame()->dump_fp_registers(false);
  } else {
    tty->print_cr("  (compiler not active)");
  }
}

extern "C" void compiler_tracer(address pc) {
  (void)pc;
}
#endif // ENABLE_COMPILER

#endif // !PRODUCT

#if !defined(PRODUCT) || ENABLE_TTY_TRACE

// Print stack, using guessed_fp as the starting address for guessing the fp.
// This is useful when we're inside compiled code or interpreter where
// the fp is not stored in Thread::current().
void psg(address guessed_fp) {
  Thread* current = Thread::current();

  BasicOop::disable_on_stack_check();

  Frame frame(current, guessed_fp);
  if (!frame.is_valid_guessed_frame()) {
    tty->print_cr("cannot print stack (unable to guess fp from 0x%x)", 
                  guessed_fp);
  } else {
    current->trace_stack_from(&frame, tty);
  }

  BasicOop::enable_on_stack_check();
}

typedef struct {
  int used;                    // 0 if we have reached the end
  int bci;
  OopDesc *compiled_method;    // NULL if the frame is an interpreted frame
  OopDesc *method;
  char name[128];
} SavedJavaStackFrame;

static SavedJavaStackFrame saved_java_stack[40];

void save_java_stack_snapshot() {
#if !ENABLE_ISOLATES
  // This code does not work in MVM -- it really should switch the task
  // first before examing the callstack of a task.
  EnforceRuntimeJavaStackDirection enfore_java_stack_direction;

  jvm_memset(saved_java_stack, 0, sizeof(saved_java_stack));
  int index = 0;
  int max = ARRAY_SIZE(saved_java_stack);

  Thread *thread = Thread::current();
  if (thread->is_null()) {
    return;
  }
  if (thread->last_java_fp() == NULL || thread->last_java_sp() == NULL) {
    return;
  }
  {
#if ENABLE_ISOLATES
    // We must switch to the context of the task
    TaskGCContext tmp(thread->task_id());
#endif
    Frame fr(Thread::current());
    while (index < max) {
      if (fr.is_entry_frame()) {
        if (fr.as_EntryFrame().is_first_frame()) {
          break;
        }
        fr.as_EntryFrame().caller_is(fr);
      } else {
        SavedJavaStackFrame *saved = &saved_java_stack[index++];
        JavaFrame java_frame = fr.as_JavaFrame();
        
        Method method = java_frame.method();
        method.print_name_to(saved->name, sizeof(saved->name));
        
        saved->used = 1;
        saved->bci = java_frame.bci();
        saved->method = method.obj();
        
#if ENABLE_COMPILER
        if (java_frame.is_compiled_frame()) {
          saved->compiled_method = java_frame.compiled_method();
        }
#endif

        java_frame.caller_is(fr);
      }
    }
  }
#endif
}

void psgc() {
  if (!Frame::in_gc_state()) {
    tty->print_cr("Not in GC state, the saved Java stack snapshot "
                  "may be stale");
  }
  tty->print_cr("Stack trace [");

  int index = 0;
  int max = ARRAY_SIZE(saved_java_stack);
  
  while (index < max) {
    SavedJavaStackFrame *saved = &saved_java_stack[index++];
    if (!saved->used) {
      break;
    }
    tty->print("    [%d] %s", index, saved->name);
    tty->fill_to(50);
    tty->print(" bci =%3d", saved->bci);
    if (saved->compiled_method) {
      tty->print(" compiled", saved->bci);
    }
    if (Verbose) {
      tty->cr();
      tty->print("        Method = 0x%x", saved->method);
      if (saved->compiled_method) {
        tty->print(", CompiledMethod = 0x%x", saved->compiled_method);
      }
    }
    tty->cr();
  }
  tty->print_cr("]");
}

#endif

// Widened to match Debug.hpp's declaration guard exactly (ENABLE_PRODUCT_PRINT_STACK
// || !defined(PRODUCT)) instead of requiring PRODUCT - the actual deployed
// binary (MIDP built with USE_DEBUG=true) links CLDC's debug variant, where
// PRODUCT is not defined, so a PRODUCT-only guard here never compiled in.
#if ENABLE_PRODUCT_PRINT_STACK || !defined(PRODUCT)

// This file is also compiled into the HOST-side loopgen/romgen tools
// (mingw32), which don't link Main_vita.cpp's real write_marker (Vita-only,
// uses sceIoOpen) - so provide the same weak self-contained fallback
// pattern already used in Interpreter_c.cpp for exactly this duality:
// real implementation wins (strong symbol) when actually building for
// the Vita target, no-op otherwise.
extern "C" {
#if __has_include(<psp2/io/fcntl.h>)
#include <psp2/io/fcntl.h>
  __attribute__((weak)) void write_marker(const char* text, int len) {
    int fd = sceIoOpen("ux0:data/renderlog.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
    if (fd >= 0) {
      sceIoWrite(fd, text, len);
      sceIoClose(fd);
    }
  }
#else
  __attribute__((weak)) void write_marker(const char* text, int len) { (void)text; (void)len; }
#endif
}

// pss()'s output through the normal `tty` Stream was confirmed to never
// reach any observable channel on the Vita port (not a crash - it just
// silently produces nothing). MarkerStream bypasses tty/DefaultStream
// entirely and writes straight through write_marker(), the one output
// path already proven reliable elsewhere in this port (renderlog.txt).
class MarkerStream : public Stream {
 public:
  MarkerStream() : Stream() {}
  virtual void print_raw(const char* s) {
    int len = 0;
    while (s[len] != 0) len++;
    write_marker(s, len);
  }
  // Stream's base class (GlobalObj) already declares a member
  // operator new(size_t), which hides the global placement-new operator
  // from lookup entirely - so MarkerStream needs its own placement
  // overload to support constructing it into a static buffer below.
  void* operator new(size_t, void* p) throw() { return p; }
};

// Lazily placement-constructed into a static POD buffer on first actual
// use (only ever reached from pss(), long after boot) - mirrors
// DefaultStream's own placement-new-into-static-buffer trick in OS.cpp,
// avoiding both early static C++ construction and any dependency on
// compiler-generated function-local-static guard variables on this
// embedded target.
static char marker_stream_storage[sizeof(MarkerStream)];
static MarkerStream* marker_stream_ptr = NULL;
static MarkerStream& marker_stream_instance() {
  if (marker_stream_ptr == NULL) {
    marker_stream_ptr = new (marker_stream_storage) MarkerStream();
  }
  return *marker_stream_ptr;
}
#define marker_stream marker_stream_instance()

static void product_trace_stack_from(Frame* frame, Stream* st) {
  st->print_cr("Stack Trace [");
  Frame fr(*frame);
  int index = 0;

  while (true) {
    if (fr.is_entry_frame()) {
      // WAS: fr.as_EntryFrame().print_on(st, index);
      if (fr.as_EntryFrame().is_first_frame()) {
        break;
      }
      fr.as_EntryFrame().caller_is(fr);
    } else {
      // WAS: fr.as_JavaFrame().print_on(st, index);
      st->print("  [%2d] ", index);
      Method::Raw m = fr.as_JavaFrame().method();
      Signature::Raw sig = m().signature();
      Symbol::Raw name = m().name();
      InstanceClass::Raw h = m().holder();
      Symbol::Raw classname = h().name();

      classname().print_symbol_on(st);
      st->print(".");
      name().print_symbol_on(st);
      sig().print_decoded_on(st);
      st->cr();
      fr.as_JavaFrame().caller_is(fr);
    }
    index ++;
  }
  st->print_cr("]");
  st->cr();
}

static void product_print_trace_do(Thread* thread, void do_oop(OopSlot*)) {
  (void)do_oop;
  marker_stream.print("[Thread: 0x%x", thread->obj());
  if (thread->obj() == Thread::current()->obj()) {
    marker_stream.print_cr(" *** CURRENT ***]");
    if (_jvm_in_quick_native_method) {
      marker_stream.print_cr("Cannot list current thread inside quick native function");
      return;
    }
  } else {
    marker_stream.print_cr("]");
  }

  Frame fr(thread);
  if (fr.fp() == 0x0) {
    marker_stream.print_cr("not started yet");
  } else {
    product_trace_stack_from(&fr, &marker_stream);
  }
}

extern "C" void pss() {
  write_marker("PSS_RAW_ENTRY\n", 14);

  MarkerStream& ms = marker_stream;
  write_marker("PSS_GOT_STREAM_REF\n", 20);

  ms.print_raw("PSS_DIRECT_PRINT_RAW\n");
  write_marker("PSS_AFTER_DIRECT_PRINT_RAW\n", 28);

  ms.print_cr("PSS_VIA_PRINT_CR");
  write_marker("PSS_AFTER_PRINT_CR\n", 20);

  marker_stream.print_cr("[Dumping all threads]");
  marker_stream.print_cr("Current thread = 0x%x", Thread::current()->obj());
  marker_stream.print_cr("");

  Scheduler::threads_do_list(product_print_trace_do, NULL,
                             Universe::global_threadlist()->obj());

  marker_stream.print_cr("[Finished dumping all threads]");
  write_marker("PSS_RAW_EXIT\n", 13);
}
#endif

#if USE_NARROW_POINTERS
// A pointer above 4GB was about to be stored in a 4-byte VM word (see
// narrow<T> in GlobalDefinitions.hpp): some memory the VM references was
// not allocated in the low arena. Stop right here - truncating it would
// corrupt the heap much later and far away.
extern "C" void narrow_pointer_overflow(const void* p) {
  // The callers' return addresses identify the culprit when no debugger is
  // attached (e.g. on the PS4: map them with the ELF and main()'s address).
  tty->print_cr("FATAL: pointer %p does not fit in a narrow (32-bit) slot", p);
  // Walk the frame-pointer chain (debug builds keep frame pointers)
  void** frame = (void**)__builtin_frame_address(0);
  for (int i = 0; i < 8 && frame != NULL; i++) {
    tty->print_cr("  called from %p", frame[1]);
    void** next = (void**)frame[0];
    if (next <= frame) {
      break;
    }
    frame = next;
  }
  BREAKPOINT;
  JVM::exit(-1);
}
#endif

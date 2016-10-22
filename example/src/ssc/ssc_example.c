#include <stdio.h>
#include <string.h>
#include <bl/base/thread.h>
#include <bl/base/time.h>
#include <bl/base/hex_string.h>
#include <ssc/simulator/simulator.h>

static define_bl_err_to_str()

/*---------------------------------------------------------------------------*/
typedef struct program {
  ssc*   sim;
  int    running;
  tstamp startup;
  char   rcv[512];
  char   send[512];
}
program;
/*---------------------------------------------------------------------------*/
void print_time (program* p, tstamp t)
{
  toffset toff = bl_tstamp_to_usec (t - p->startup);
  toffset sec  = toff / usec_in_sec;
  toffset usec = toff % usec_in_sec;
  /*TODO: provide tstamp formatters in time.h*/
  printf ("[%05u.%06u]", sec, usec);
}
/*---------------------------------------------------------------------------*/
void process_read_message (program* p, ssc_output_data* od)
{
  print_time (p, od->time);
  switch ((od->type & ~ssc_type_is_dynamic_mask)) {
  case ssc_type_bytes: {
    memr16 data = ssc_output_read_as_bytes (od);
    int ret = bl_bytes_to_hex_string(
      p->rcv, arr_elems (p->rcv), memr16_beg (data), memr16_size (data)
      );
    if (-1 == ret) {
      fprintf (stderr, "<- message too big\n");
    }
    printf ("<- [dyn:%d] %s\n", ssc_output_is_dynamic (od), p->rcv);
    break;
  }
  case ssc_type_string: {
    u16 strlength;
    char const* str = ssc_output_read_as_string (od, &strlength);
    printf ("<- [dyn:%d] %s\n", ssc_output_is_dynamic (od), str);
    break;
  }
  case ssc_type_error: {
    bl_err      err;
    char const* errstr;
    ssc_output_read_as_error (od, &err, &errstr);
    printf ("<- err: %s, errstr: %s\n", bl_err_to_str (err), errstr);
    break;
  }
  default: break;
  }
}
/*---------------------------------------------------------------------------*/
int write_console (void* context)
{
  program* p = (program*) context;
  size_t size;
  while (p->running) {
    char* line = fgets (&p->send, arr_elems (p->send), stdin);
    if (!line) {
      fprintf (stderr, "fgets failed\n");
      continue;
    }
    char *lf_ptr = strchr (line, '\n');
    if (lf_ptr) {
      *lf_ptr = 0;
    }
    uword size = strlen (line);
    if (size == 0) {
      continue;
    }
    if (strcmp (line, "exit") == 0) {
      p->running = 0;
      return 0;
    }
    uword bytes = (size / 2) + (size & 1);
    u8* mem = ssc_alloc_write_bytestream (p->sim, bytes);
    if (!mem) {
      fprintf (stderr, "ssc_alloc_write_bytestream failed!\n");
      continue;
    }
    /*printf ("strlen: %u, bytes: %u, line: %s\n", size, bytes, line);*/
    word ret = bl_hex_string_to_bytes (mem, bytes, line);
    if (ret < 0) {
      fprintf (stderr, "invalid hex string\n");
      continue;
    }
    print_time (p, bl_get_tstamp());
    bl_err err = ssc_write (p->sim, 0, mem, (u16) ret);
    if (!err) {
      printf ("-> %s\n", line);
    }
    else {
      fprintf (stderr, "-> ssc_write failed: %s\n", bl_err_to_str (err));
    }
  }
  return -1;
}
/*---------------------------------------------------------------------------*/
int main (int argc, char const* argv[])
{
  struct program p;
  p.running         = 1;
  uword timebase_us = 10000000;
  bl_err err        = ssc_create (&p.sim, "", &timebase_us);
  if (err) {
    fprintf (stderr, "unable to create ssc_lua: %s\n", bl_err_to_str (err));
    return (int) err;
  }
  p.startup = bl_get_tstamp();
  err       = ssc_run_setup (p.sim);
  if (err) {
    fprintf (stderr, "unable to run ssc_lua setup: %s\n", bl_err_to_str (err));
    goto destroy;
  }
  printf(
    "-------------------------------------------------------------\n"
    " Type hex strings followed by \"Enter\" to send them to the\n"
    " simulator. Type \"exit\" to quit the program\n"
    "-------------------------------------------------------------\n"
    );
  bl_thread thr;
  int thr_err = bl_thread_init (&thr, write_console, &p);
  if (thr_err != 0) {
    fprintf (stderr, "unable to start console thread\n");
    goto teardown;
  }
  ssc_output_data od[16];
  while (p.running) {
    err = ssc_run_some (p.sim, 100000);
    if (err && err != bl_timeout) {
      /*TODO*/
    }
    uword read_msgs;
    (void) ssc_read (p.sim, &read_msgs, od, arr_elems (od), 0);
    for (uword i = 0; i < read_msgs; ++i) {
      process_read_message (&p, od + i);
      ssc_dealloc_read_data (p.sim, od + i);
    }
  }
teardown:
  ssc_run_teardown (p.sim);
destroy:
  ssc_destroy (p.sim);
  return (int) err;
}
/*---------------------------------------------------------------------------*/

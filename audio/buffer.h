/* buffer.h
   audio lib
   aleph

   classes for manipulating audio buffers.

*/

#ifndef _ALEPH_BUFFER_H_
#define _ALEPH_BUFFER_H_

#include "fix.h"
#include "fix32.h"
#include "types.h"

//--- audioBuffer
// data+descriptor class.
typedef struct _audioBuffer {
  // count of frames
  u32 frames;
  // duration in seconds 
  fix16 dur;
  // pointer to data
  fract32 *data;
} audioBuffer;


// ---- bufferTap
// class for creating a "tap" or "head."
 // stores position/rate within a buffer.
// supports interpolated read/write.
typedef struct _bufferTap {
  // pointer to buf
  audioBuffer* buf;
  // index to loop at (upper bound)
  u32 loop;
  // current index
  fix32 idx;
  // phase increment 
  fix32 inc;
} bufferTap;


// ---- bufferTapN
// class for creating a "tap" or "head."
 // stores position/rate within a buffer.
// non-interpolated.
typedef struct _bufferTapN {
  // pointer to buf
  audioBuffer* buf;
  // index to loop at (upper bound)
  u32 loop;
  // current index
  u32 idx;
  // phase increment 
  u32 inc;
  // rate divisor
  u32 div;
  // current divisor count
  u32 divCount;
} bufferTapN;

// initialize a (mono) audio buffer at pre-allocated memory.
// provide 2nd pointer for data
// so it can be placed in an arbitrarily separate memory region.
void buffer_init(audioBuffer* buf, fract32* data, u32 frames);

// intialize tap
void buffer_tap_init(bufferTap* tap, audioBuffer* buf);

// increment the index in a tap
void buffer_tap_next(bufferTap* tap);

// interpolated read
fract32 buffer_tap_read(bufferTap* tap);

// interpolated write (erases old contents)
void buffer_tap_write(bufferTap* tap, fract32 val); 

// interpolated mix (old + new, arbitrary)
void buffer_tap_mix(bufferTap* tap, fract32 val, fract32 preLevel); 

// interpolated add (new + old (unchanged)
void buffer_tap_add(bufferTap* tap, fract32 val);

// set rate
void buffer_tap_set_rate(bufferTap *tap, fix16 rate);

// set a different buffer (resets position)
void buffer_tap_set_buf(bufferTap* tap, audioBuffer* buf);

// set loop endpoint in seconds
void buffer_tap_set_loop(bufferTap* tap, fix16 sec);

// synchronize one tap with another at a given offset in seconds.
// useful for delays
void buffer_tap_sync(bufferTap* tap, bufferTap* target, fix16 offset);
 
// set tap position directly
void buffer_tap_set_pos(bufferTap* tap, fix16 secs);

//--------------------------------------------------------
//---------------------------------------------------------
//---- non-interpolated version!

// intialize
void buffer_tapN_init(bufferTapN* tap, audioBuffer* buf);

// increment the index in a tap
void buffer_tapN_next(bufferTapN* tap);


// non-interpolated read
fract32 buffer_tapN_read(bufferTapN* tap);

// non-interpolated write (erases old contents)
void buffer_tapN_write(bufferTapN* tap, fract32 val); 

// non-interpolated mix (old + new, arbitrary)
void buffer_tapN_mix(bufferTapN* tap, fract32 val, fract32 preLevel); 

// non-interpolated add (new + old (unchanged)
void buffer_tapN_add(bufferTapN* tap, fract32 val);

// set rate
//void buffer_tapN_set_rate(bufferTapN *tap, fix16 rate);

// set per-sample increment
void buffer_tapN_set_inc(bufferTapN *tap, u32 inc);

// set rate divisor
void buffer_tapN_set_div(bufferTapN *tap, u32 div);

// set a different buffer (resets position)
void buffer_tapN_set_buf(bufferTapN* tap, audioBuffer* buf);

// set loop endpoint in seconds
void buffer_tapN_set_loop(bufferTapN* tap, fix16 sec);

// synchronize one tap with another at a given offset in seconds.
// useful for delays
void buffer_tapN_sync(bufferTapN* tap, bufferTapN* target, fix16 offset);

// set tap position directly
 void buffer_tapN_set_pos(bufferTapN* tap, fix16 secs);


#endif // h guard

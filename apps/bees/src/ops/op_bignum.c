// avr32-lib
#include "region.h"
#include "screen.h"

// bees
#include "app_timers.h"
#include "net_poll.h"
#include "net_protected.h"
#include "op_gfx.h"
#include "pages.h"
#include "print_funcs.h"
#include "pickle.h"
#include "op_bignum.h"


//-------------------------------------------------
//----- static vars

//-- descriptor
static const char* op_bignum_instring = "ENABLE  PERIOD  VAL     X       Y       ";
static const char* op_bignum_outstring = "";
static const char* op_bignum_opstring = "BIGNUM";

//-- temp
// string buffer for number->text rendering
static char tmpStr[16];
// try sharing region data among instances...
//static volatile u8 regData[OP_BIGNUM_GFX_BYTES];
//// ^^^ fuck. i think that is broken. try moving to class structure and allocating from op poll?


//-------------------------------------------------
//----- static function declaration

// UI increment
static void op_bignum_inc(op_bignum_t* bignum, const s16 idx, const io_t inc);
// set inputs
static void op_bignum_in_enable(op_bignum_t* bignum, const io_t v);
static void op_bignum_in_period(op_bignum_t* bignum, const io_t v);
static void op_bignum_in_val(op_bignum_t* bignum, const io_t v);
static void op_bignum_in_x(op_bignum_t* bignum, const io_t x);
static void op_bignum_in_y(op_bignum_t* bignum, const io_t y);

// pickle / unpickle
static u8* op_bignum_pickle(op_bignum_t* bignum, u8* dst);
static u8* op_bignum_unpickle(op_bignum_t* bignum, const u8* src);


//// redraw
static void op_bignum_redraw(op_bignum_t* bignum);

// timer manipulation
static inline void op_bignum_set_timer(op_bignum_t* bignum);
static inline void op_bignum_unset_timer(op_bignum_t* bignum);

// polled-operator handler
void op_bignum_poll_handler(void* op);

// array of input functions 
static op_in_fn op_bignum_in[5] = {
  (op_in_fn)&op_bignum_in_enable,
  (op_in_fn)&op_bignum_in_period,
  (op_in_fn)&op_bignum_in_val,
  (op_in_fn)&op_bignum_in_x,
  (op_in_fn)&op_bignum_in_y,
};

//----- external function definition

/// initialize
void op_bignum_init(void* op) {
  op_bignum_t* bignum = (op_bignum_t*)op;

  // superclass functions
  bignum->super.inc_fn = (op_inc_fn)&op_bignum_inc;
  bignum->super.in_fn = op_bignum_in;
  bignum->super.pickle = (op_pickle_fn) (&op_bignum_pickle);
  bignum->super.unpickle = (op_unpickle_fn) (&op_bignum_unpickle);
    // polled operator superclass
  bignum->op_poll.handler = (poll_handler_t)(&op_bignum_poll_handler);
  bignum->op_poll.op = bignum;

  // superclass val
  bignum->super.numInputs = 5;
  bignum->super.numOutputs = 0;
 
  bignum->super.in_val = bignum->in_val;
  bignum->in_val[0] = &(bignum->enable);
  bignum->in_val[1] = &(bignum->period);
  bignum->in_val[2] = &(bignum->val);
  bignum->in_val[3] = &(bignum->x);
  bignum->in_val[4] = &(bignum->y);

  bignum->super.out = bignum->outs;
  bignum->super.opString = op_bignum_opstring;
  bignum->super.inString = op_bignum_instring;
  bignum->super.outString = op_bignum_outstring;
  bignum->super.type = eOpBignum;

  // class val
  bignum->enable = 0;
  bignum->period = op_from_int(50);
  bignum->val = 0;
  bignum->x = 0;
  bignum->y = 0;

  // init graphics
  /*.. this is sort of retarded, 
  doing stuff normally accomplished in region_alloc() or in static init,
  but doing a dumb memory hack on it to share a tmp data space between instances.
  */  
  bignum->reg.dirty = 0;
  bignum->reg.x = 0;
  bignum->reg.y = 0;
  bignum->reg.w = OP_BIGNUM_PX_W;
  bignum->reg.h = OP_BIGNUM_PX_H;
  bignum->reg.len = OP_BIGNUM_GFX_BYTES;
  bignum->reg.data = (u8*) (bignum->regData);

  region_fill(&(bignum->reg), 0);
}


/// de-initialize
void op_bignum_deinit(void* op) {
  op_bignum_t* bignum = (op_bignum_t*)op;
  if(bignum->enable > 0) {
    op_gfx_disable();
    op_bignum_unset_timer(bignum);
  }
}

//-------------------------------------------------
//----- static function definition

//===== operator input
void op_bignum_in_enable(op_bignum_t* bignum, const io_t v  ) {
  if(v > 0) {
    if(bignum->enable > 0) {
      ;;
    } else {
      op_gfx_enable();
      bignum->enable = 1;
      op_bignum_set_timer(bignum);
    }
  } else { 
    if(bignum->enable > 0) {
      op_gfx_disable();
      bignum->enable = 0;
      op_bignum_unset_timer(bignum);
    } else {
      ;;
    }
  }
}

// input period
void op_bignum_in_period(op_bignum_t* bignum, const io_t v) {
  if((v) < 5) {
    bignum->period = 5;
  } else {
    bignum->period = v;
  }
  bignum->timer.ticks = op_to_int(bignum->period);
}

// input val
void op_bignum_in_val(op_bignum_t* bignum, const io_t v) {
  bignum->val = v;
  op_bignum_redraw(bignum);
}

// input x position
void op_bignum_in_x(op_bignum_t* bignum, const io_t v) {
  // blank so we don't leave a trail
  region_fill(&(bignum->reg), 0);
  if (v > OP_BIGNUM_X_MAX) {
    bignum->x = bignum->reg.x = OP_BIGNUM_X_MAX; 
  } else {		
    bignum->x = bignum->reg.x = v;
  }
  op_bignum_redraw(bignum);
}

// input y position
void op_bignum_in_y(op_bignum_t* bignum, const io_t v) {
  // blank so we don't leave a trail
  if (v > OP_BIGNUM_Y_MAX) {
    bignum->y = bignum->reg.y = OP_BIGNUM_Y_MAX;
  } else {		
    bignum->y = bignum->reg.y = v;
  }
  op_bignum_redraw(bignum);
}



// poll event handler
void op_bignum_poll_handler(void* op) {
  op_bignum_t* bignum = (op_bignum_t*)op;
  region* r = &(bignum->reg);
  if(pageIdx == ePagePlay) {
    if(r->dirty) {
      screen_draw_region(r->x, r->y, r->w, r->h, r->data);
      r->dirty = 0;
    }
  }
}

//===== UI input

// increment
static void op_bignum_inc(op_bignum_t* bignum, const s16 idx, const io_t inc) {
  io_t val;
  switch(idx) {
  case 0: // enable
    op_bignum_in_enable(bignum, inc);
    break;
  case 1: // period
    val = op_sadd(bignum->period, inc);
    op_bignum_in_period(bignum, val);
    break;
  case 2: // current value
    val = op_sadd(bignum->val, inc);
    op_bignum_in_val(bignum, val);
    break;
  case 3: // x position
    val = op_sadd(bignum->x, inc);
    op_bignum_in_x(bignum, val);
    break;
  case 4: // y position
    val = op_sadd(bignum->x, inc);
    op_bignum_in_y(bignum, val);
    break;
  }
}


// serialization
u8* op_bignum_pickle(op_bignum_t* bignum, u8* dst) {
  // store val variables
  dst = pickle_io(bignum->enable, dst);
  dst = pickle_io(bignum->period, dst);
  dst = pickle_io(bignum->val, dst);
  dst = pickle_io(bignum->x, dst);
  dst = pickle_io(bignum->y, dst);
  return dst;
}

u8* op_bignum_unpickle(op_bignum_t* bignum, const u8* src) {
  // retreive val  variables
  //////////////
  //// FIXME: i think this is wrong... need to re-set the input value for reregistration? 
  src = unpickle_io(src, (u32*)&(bignum->enable));
  //////////////
  src = unpickle_io(src, (u32*)&(bignum->period));
  src = unpickle_io(src, (u32*)&(bignum->val));
  src = unpickle_io(src, (u32*)&(bignum->x));
  src = unpickle_io(src, (u32*)&(bignum->y));
   return (u8*)src;
}

// redraw with current state
void op_bignum_redraw(op_bignum_t* bignum) {
  //// TEST
  /* u32 i, j; */
  /* u8* dat = &(bignum->reg.data[0]); */
  //  region* r = &(bignum->reg);

  if(bignum->enable <= 0) { return; }

  // print value to text buffer
  op_print(tmpStr, bignum->val);

  print_dbg("\r\n op_bignum_redraw , ");
  print_dbg(tmpStr);

  // blank
  region_fill(&(bignum->reg), 0);

  // render text to region
  region_string_aa(&(bignum->reg), tmpStr, 0, 0, 1);

  // ascii art
    /* print_dbg("\r\n"); */
    /* for(i=0; i< OP_BIGNUM_PX_H; i++) { */
    /*   for(j=0; j< OP_BIGNUM_PX_W; j++) { */
    /* 	if(*dat > 0) { print_dbg("#"); } else { print_dbg("_"); } */
    /* 	dat++; */
    /*   } */
    /*   print_dbg("\r\n"); */
    /* } */

    // FIXME: this should NOT go here. 
  //gfx ops should inherit from op_poll,
  //  and only call for a redraw every 100 ms or whatever.
  //    screen_draw_region(r->x, r->y, r->w, r->h, r->data);
}


// timer manipulation
static inline void op_bignum_set_timer(op_bignum_t* bignum) {
  timers_set_custom(&(bignum->timer), op_to_int(bignum->period), &(bignum->op_poll) );
}

static inline void op_bignum_unset_timer(op_bignum_t* bignum) {
  timer_remove(&(bignum->timer));
  timers_unset_custom(&(bignum->timer));
}

/* svfnoise.c
   aleph-bfin
   
   noise generator + input + state variable filter

*/

// std
#include <stdlib.h>
#include <string.h>
// aleph-common
#include "fix.h"
// aleph-audio
#include "conversion.h"
#include "env.h"
#include "filter_svf.h"
#include "noise.h"
// bfin
#ifdef ARCH_BFIN
#include "bfin_core.h"
#include "fract_math.h"
#include <fract2float_conv.h>
#else
// linux
#include <stdio.h>
#include "fract32_emu.h"
#warning "linux"
#include "audio.h"
#endif

#include "module.h"
#include "module_custom.h"
#include "types.h"

#define HZ_MIN 0x200000      // 32
#define HZ_MAX 0x40000000    // 16384


// define a local data structure that subclasses moduleData.
// use this for all data that is large and/or not speed-critical.
// this structure should statically allocate all necessary memory 
// so it can simply be loaded at the start of SDRAM.
typedef struct _drumsData {
  moduleData super;
  ParamDesc mParamDesc[eParamNumParams];
  ParamData mParamData[eParamNumParams];
} drumsData;


//-------------------------
//----- extern vars (initialized here)
moduleData * gModuleData; // module data

//-----------------------
//------ static variables
#if ARCH_LINUX
static FILE* dbgFile;
u8 dbgFlag = 0;
u32 dbgCount = 0;
#endif


// pointer to local module data, initialize at top of SDRAM
static drumsData * data;

//-- static allocation (SRAM) for variables that are small and/or frequently accessed:
// two random number generators for low/high words of very white noise:
static lcprng* rngH;
static lcprng* rngL;
// filter
static filter_svf* svf;
// amp envelope
static env_asr*  ampEnv;
// parameters
static fract32 inAmp0;
static fract32 inAmp1;
static fract32 inAmp2;
static fract32 inAmp3;
static fract32 noiseAmp;
// output
static fract32 frameVal;

//-----------------------------
//----- static functions

// get the next value of white noise
static fract32 noise_next(void) {
  fract32 x = 0;
  x |= lcprng_next(rngH);
  x |= ((lcprng_next(rngL) >> 16) & 0x0000ffff);
  return x;
}

// frame calculation
static void calc_frame(void) {
  frameVal = 0;
  frameVal = mult_fr1x32x32(noise_next(), noiseAmp);
  frameVal = add_fr1x32(frameVal, mult_fr1x32x32(in[0], inAmp0));
  frameVal = add_fr1x32(frameVal, mult_fr1x32x32(in[1], inAmp1));
  frameVal = add_fr1x32(frameVal, mult_fr1x32x32(in[2], inAmp2));
  frameVal = add_fr1x32(frameVal, mult_fr1x32x32(in[3], inAmp3));
  frameVal = filter_svf_next(svf, frameVal);
  frameVal = mult_fr1x32x32(frameVal, env_asr_next(ampEnv));
}

//----------------------
//----- external functions

void module_init(void) {
  // init module/param descriptor
#ifdef ARCH_BFIN 
  // intialize local data at start of SDRAM
  data = (drumsData * )SDRAM_ADDRESS;
  // initialize moduleData superclass for core routines
#else
  data = (drumsData*)malloc(sizeof(drumsData));
  /// debugging output file
  dbgFile = fopen( "iotest_dbg.txt", "w");
#endif
  gModuleData = &(data->super);
  gModuleData->paramDesc = data->mParamDesc;
  gModuleData->paramData = data->mParamData;
  gModuleData->numParams = eParamNumParams;

  fill_param_desc();

  inAmp0 = 0;
  inAmp1 = 0;
  inAmp2 = 0;
  inAmp3 = 0;
  noiseAmp = fix16_one >> 2;

  svf = (filter_svf*)malloc(sizeof(filter_svf));
  filter_svf_init(svf);

  rngH = (lcprng*)malloc(sizeof(lcprng));
  lcprng_reset(rngH);

  rngL = (lcprng*)malloc(sizeof(lcprng));
  lcprng_reset(rngL);
  
  // allocate envelope
  ampEnv = (env_asr*)malloc(sizeof(env_asr));
  env_asr_init(ampEnv);

  // initial param state
  filter_svf_set_hz(svf, fix16_from_int(220));
  filter_svf_set_rq(svf, 0x4000);
  filter_svf_set_low(svf, 0x4000);

  env_asr_set_atk_shape(ampEnv, float_to_fr32(0.5));
  env_asr_set_rel_shape(ampEnv, float_to_fr32(0.5));
  env_asr_set_atk_dur(ampEnv, 1000);
  env_asr_set_rel_dur(ampEnv, 10000);


}

void module_deinit(void) {
  free(svf);
  free(rngH);
  free(rngL);
#if ARCH_LINUX 
  fclose(dbgFile);
#endif
}


// set parameter by value (fix16)
void module_set_param(u32 idx, pval v) {
  switch(idx) {
  case eParamGate:
     env_asr_set_gate(ampEnv, v.s > 0);
    break;
  case eParamSvfHz :
    filter_svf_set_hz(svf, v.fix);
    break;
  case eParamSvfRq :
    filter_svf_set_rq(svf, FIX16_FRACT_TRUNC(v.fix));
    break;
  case eParamSvfLow :
    filter_svf_set_low(svf, FIX16_FRACT_TRUNC(v.fix));
    break;
  case eParamSvfHigh :
    filter_svf_set_high(svf, FIX16_FRACT_TRUNC(v.fix));
    break;
  case eParamSvfBand :
    filter_svf_set_band(svf, FIX16_FRACT_TRUNC(v.fix));
    break;
  case eParamSvfNotch :
    filter_svf_set_notch(svf, FIX16_FRACT_TRUNC(v.fix));
    break;
  case eParamSvfPeak :
    filter_svf_set_peak(svf, FIX16_FRACT_TRUNC(v.fix));
    break;
  case eParamNoiseAmp :
    noiseAmp = FIX16_FRACT_TRUNC(v.fix);
    break;
  case eParamInAmp0 :
    inAmp0 = FIX16_FRACT_TRUNC(v.fix);
    break;
  case eParamInAmp1 :
    inAmp0 = FIX16_FRACT_TRUNC(v.fix);
    break;
  case eParamInAmp2 :
    inAmp0 = FIX16_FRACT_TRUNC(v.fix);
    break;
  case eParamInAmp3 :
    inAmp0 = FIX16_FRACT_TRUNC(v.fix);
    break;
  case eParamAtkDur:
    env_asr_set_atk_dur(ampEnv, sec_to_frames_trunc(v.fix));
    break;
  case eParamRelDur:
    env_asr_set_rel_dur(ampEnv, sec_to_frames_trunc(v.fix));
    break;
  case eParamAtkCurve:
    env_asr_set_atk_shape(ampEnv, FIX16_FRACT_TRUNC(BIT_ABS(v.fix)));
    break;
  case eParamRelCurve:
    env_asr_set_atk_shape(ampEnv, FIX16_FRACT_TRUNC(BIT_ABS(v.fix)));
    break;

  default:
    break;
  }
}

// get number of parameters
extern u32 module_get_num_params(void) {
  return eParamNumParams;
}

// frame callback
#ifdef ARCH_BFIN 
void module_process_frame(void) {
  calc_frame();
  out[0] = (frameVal);
  out[1] = (frameVal);
  out[2] = (frameVal);
  out[3] = (frameVal);
}

#else //  non-bfin
void module_process_frame(const f32* in, f32* out) {
  u32 frame;
  u8 chan;
  for(frame=0; frame<BLOCKSIZE; frame++) {
    calc_frame(); 
    for(chan=0; chan<NUMCHANNELS; chan++) { // stereo interleaved
      // FIXME: could use fract for output directly (portaudio setting?)
      *out = fr32_to_float(frameVal);
      if(dbgFlag) {  
	/* fprintf(dbgFile, "%d \t %f \t %f \t %f \r\n",  */
	/* 	dbgCount,  */
	/* 	*out */
	/* 	//		fr32_to_float(osc2), */
	/* 	//		fr32_to_float((fract32)modIdxOffset << 3) */
	/* 	//		fr32_to_float((fract32)modIdxOffset << 16) */
	/* 	//		fr32_to_float((fract32)modIdxOffset) */
	/* 	); */
	dbgCount++;
      }
      out++;
    }
  }
}
#endif

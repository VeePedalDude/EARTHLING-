// -----------------------------------------------------------------------------
// Created by VeePedalDude - veepedaldude@gmail.com
// Free and open source, shared with the DIY pedal community for the love of the
// craft. I make nothing from it. Questions, ideas, or just want to connect?
// Email me anytime.
// -----------------------------------------------------------------------------
// =============================================================================
// Dragon Curve Fractal Delay Engine
// =============================================================================
//
// L-System:
//   Axiom : F
//   Rules : F -> F+G   |   G -> F-G
//
//   F  =  Tap 0  (primary delay)
//   G  =  Tap 1  (secondary delay)
//   +  =  Turn right -> Tap 2, pitch UP
//   -  =  Turn left  -> Tap 2, pitch DOWN
//
// Toggle switches (LOW = ON):
//   D7   Turn Bloom     - extra grain burst on every turn event
//   D8   Resonant Body  - dual-bandpass coloration layer
//   D9   Retrace        - reversed / mirrored grains on turns and cycle wraps
//   D10  Fast Swell     - violin-style clean-path attack shaping
//
// Performance buttons + LEDs:
//   D26 / D23  Geometry Hold (tap = latch, hold = momentary)
//   D25 / D22  Bypass        (tap = latch, hold = momentary)
//
// Knobs:
//   A1  Time      - base delay time
//   A2  Feedback  - overall feedback level
//   A3  Mix       - dry / wet balance
//   A4  Structure - quantised L-System generation + slice subdivision
//   A5  Govern    - repeat steering and bloom strength; pitch amount from mid
//   A6  Drive+Boost (shared) - drive over the lower half, boost over the upper
//
// =============================================================================
// Forward declarations required by the Arduino pre-processor
struct Grain;
struct LSystemOut;

#include "DaisyDuino.h"
#include <cmath>
#include <cstdint>
#include <cstring>

// --- Gain staging ---
static const float INPUT_PRE_GAIN       = 2.00f;
static const float OUTPUT_POST_GAIN     = 2.00f;
static const float OUTPUT_BOOST_MAX_DB  = 16.0f;
static const float MASTER_OUTPUT_GAIN   = 1.60f;

// --- Delay buffer ---
static const int   DELAY_BUFFER_LEN     = 144000;
static const int   DELAY_GUARD_SAMPLES  = 256;
static const float DELAY_TIME_MIN_MS    = 80.f;
static const float DELAY_TIME_MAX_MS    = 900.f;
static const float TAP_B_DELAY_RATIO    = 0.6667f;
static const float TAP_C_DELAY_RATIO    = 1.3333f;
static const float TAP_D_DELAY_RATIO    = 2.0000f;
// Geometry offsets the secondary taps in time, so the curve is audible in the
// delay itself and not only in the grain layer.
static const float TAP_GEOM_MOD_MS      = 12.f;
static const float TAP_GEOM_SLEW        = 0.00012f;
// Time knob glide: the main tap follows the knob smoothly (tape-style bend)
// instead of jumping whole samples, which clicked. ms, safe 10..300.
static const float TAP_TIME_GLIDE_MS    = 60.f;

// --- L-System ---
static const int   LSYS_BUFFER_LEN      = 512;

// --- Structure / slice clock ---
static const int   STRUCTURE_GEN_TABLE[4]        = {2, 3, 4, 5};
static const int   STRUCTURE_SLICE_DIV_TABLE[4]  = {4, 6, 8, 12};
static const float SLICE_PERIOD_MIN_SAMPS        = 96.f;
static const float SLICE_PERIOD_MAX_SAMPS        = 6000.f;

// --- Dragon Curve turtle ---
static const int   TURTLE_HISTORY_LEN       = 32;
static const int   SPATIAL_GRID_SIZE        = 16;
static const int   SPATIAL_CELL_SHIFT       = 1;
static const float SPATIAL_SPREAD_NORM_CELLS = 48.f;

static const float SELF_NEAR_SCORE_DIST_1   = 1.00f;
static const float SELF_NEAR_SCORE_DIST_2   = 0.72f;
static const float SELF_NEAR_SCORE_DIST_3   = 0.46f;
static const float SELF_NEAR_SCORE_DIST_4   = 0.24f;
static const float SELF_NEAR_SCORE_DISTANT  = 0.00f;

static const float CELL_DENSITY_SATURATION_VISITS = 5.f;

static const float TURN_DENSITY_PER_STEP_DECAY  = 0.82f;
static const float TURN_DENSITY_ON_TURN_DECAY   = 0.88f;
static const float TURN_DENSITY_PER_TURN_GAIN   = 1.00f;
static const float TURN_DENSITY_MAX             = 8.00f;
static const float TURN_DENSITY_DISPLAY_SCALE   = 0.25f;
static const float SELF_NEAR_ON_TURN_DECAY      = 0.96f;
static const float SELF_NEAR_LERP_RATE          = 0.34f;

static const float CYCLE_MEM_VISIT_ACCUM_RATE   = 0.18f;
static const float CYCLE_MEM_NEAR_LERP_RATE     = 0.28f;
static const float CYCLE_MEM_SPREAD_LERP_RATE   = 0.12f;
static const float CYCLE_MEM_BIAS_LERP_RATE     = 0.10f;
static const float CYCLE_MEM_NEIGHBOR_DECAY     = 0.75f;
static const float CYCLE_MEM_ACTIVE_THRESHOLD   = 0.12f;
static const float CYCLE_MEM_WRAP_DECAY         = 0.92f;

static const float BLEND_SELF_NEAR_INSTANT  = 0.72f;
static const float BLEND_SELF_NEAR_MEMORY   = 0.28f;
static const float BLEND_SPREAD_INSTANT     = 0.74f;
static const float BLEND_SPREAD_MEMORY      = 0.26f;
static const float BLEND_QUAD_BIAS_INSTANT  = 0.80f;
static const float BLEND_QUAD_BIAS_MEMORY   = 0.20f;

static const float PITCH_RATIO_UNISON  = 1.0000f;
static const float PITCH_RATIO_FIFTH   = 1.4983f;
static const float PITCH_RATIO_OCTAVE  = 2.0000f;

static const float PITCH_LUT[8] = {
  0.5000f, 0.6667f, 0.7492f, 1.0000f,
  1.3348f, 1.4983f, 2.0000f, 1.0000f
};

static const float BODY_BASE_FREQ_EAST  = 340.0f;
static const float BODY_BASE_FREQ_NORTH = 520.0f;
static const float BODY_BASE_FREQ_WEST  = 760.0f;
static const float BODY_BASE_FREQ_SOUTH = 1120.0f;
static const float BODY_HEADING_FREQS[4] = {
  BODY_BASE_FREQ_EAST, BODY_BASE_FREQ_NORTH, BODY_BASE_FREQ_WEST, BODY_BASE_FREQ_SOUTH
};
static const float BODY_FREQ_RADIUS_SCALE_LO = 0.92f;
static const float BODY_FREQ_RADIUS_SCALE_HI = 1.28f;
static const float BODY_FREQ_CLAMP_MIN        = 180.f;
static const float BODY_FREQ_CLAMP_MAX        = 3400.f;
static const float BODY_FREQ2_CLAMP_MIN       = 260.f;
static const float BODY_FREQ2_CLAMP_MAX       = 5200.f;
static const float BODY_PARAM_SLEW            = 0.22f;

static const float SHAPER_PARAM_SLEW       = 0.14f;
// The shaper's settings change once per slice; each change glides in over this
// many ms so the steps are not written into the delay. ms, safe 1..20.
static const float SHAPER_GLIDE_MS         = 5.f;
static const float SHAPER_DRIVE_USER_SCALE = 0.90f;
static const float DC_BLOCKER_CUTOFF_HZ    = 18.0f;

static const float GRAIN_LEN_NORMAL_MUL       = 1.40f;
static const float GRAIN_LEN_FIFTH_MUL        = 3.00f;
static const float GRAIN_LEN_OCTAVE_MUL       = 1.20f;
static const float GRAIN_LEN_BLOOM_B_MUL      = 0.95f;
static const float GRAIN_LEN_BLOOM_C_MUL      = 0.70f;
static const float GRAIN_LEN_RETRACE_A_MUL    = 0.75f;
static const float GRAIN_LEN_RETRACE_B_MUL    = 0.95f;
static const float GRAIN_LEN_RETRACE_C_MUL    = 0.80f;
static const float GRAIN_LEN_WRAP_A_MUL       = 1.25f;
static const float GRAIN_LEN_WRAP_B_MUL       = 1.05f;
static const float GRAIN_LEN_WRAP_C_MUL       = 0.90f;
static const float GRAIN_LEN_DEEP_MUL         = 4.00f;

static const float GRAIN_LEN_NORMAL_MIN   = 220.f;  static const float GRAIN_LEN_NORMAL_MAX   = 1800.f;
static const float GRAIN_LEN_FIFTH_MIN    = 420.f;  static const float GRAIN_LEN_FIFTH_MAX    = 3200.f;
static const float GRAIN_LEN_OCTAVE_MIN   = 180.f;  static const float GRAIN_LEN_OCTAVE_MAX   = 1100.f;
static const float GRAIN_LEN_BLOOM_MIN    = 90.f;   static const float GRAIN_LEN_BLOOM_MAX    = 900.f;
static const float GRAIN_LEN_RETRACE_MIN  = 90.f;   static const float GRAIN_LEN_RETRACE_MAX  = 900.f;
static const float GRAIN_LEN_WRAP_MIN     = 110.f;  static const float GRAIN_LEN_WRAP_MAX     = 1200.f;
static const float GRAIN_LEN_DEEP_MIN     = 700.f;  static const float GRAIN_LEN_DEEP_MAX     = 4200.f;

static const float REPEAT_BASE_WEIGHTS[4][3] = {
  {1.10f, 0.34f, 0.18f},
  {0.76f, 0.90f, 0.22f},
  {0.84f, 0.72f, 0.32f},
  {0.62f, 0.30f, 0.98f}
};

static const float REPEAT_WEIGHT_SLEW_RATE = 0.0045f;
static const float FB_WEIGHT_SLEW_RATE     = 0.0035f;

static const float LANE_A_CENTER_WEIGHT    =  0.60f;
static const float LANE_A_SPREAD_WEIGHT    =  0.25f;
static const float LANE_A_FRONTIER_WEIGHT  = -0.22f;
static const float LANE_A_CELL_WEIGHT      = -0.18f;
static const float LANE_A_AXIS_WEIGHT      = -0.16f;
static const float LANE_A_CENTROID_WEIGHT  = -0.18f;
static const float LANE_A_CLAMP_LO         = -0.60f;
static const float LANE_A_CLAMP_HI         =  0.80f;

static const float LANE_B_RADIUS_WEIGHT    = 0.42f;
static const float LANE_B_NEAR_WEIGHT      = 0.34f;
static const float LANE_B_CELL_WEIGHT      = 0.22f;
static const float LANE_B_SPREAD_WEIGHT    = 0.18f;
static const float LANE_B_AXIS_WEIGHT      = 0.26f;
static const float LANE_B_CENTROID_WEIGHT  = 0.18f;
static const float LANE_B_CLAMP_LO         = -0.30f;
static const float LANE_B_CLAMP_HI         =  0.90f;

static const float LANE_C_TURN_WEIGHT      = 0.40f;
static const float LANE_C_NEAR_WEIGHT      = 0.38f;
static const float LANE_C_ASPECT_WEIGHT    = 0.16f;
static const float LANE_C_EDGE_WEIGHT      = 0.22f;
static const float LANE_C_FRONTIER_WEIGHT  = 0.18f;
static const float LANE_C_CELL_WEIGHT      = 0.12f;
static const float LANE_C_AXIS_WEIGHT      = 0.18f;
static const float LANE_C_CENTROID_WEIGHT  = 0.20f;
static const float LANE_C_CLAMP_LO         = -0.30f;
static const float LANE_C_CLAMP_HI         =  1.00f;

static const float MIX_WET_DIRECT_SCALE  = 0.62f;
static const float MIX_WET_OCTAVE_SCALE  = 0.54f;
static const float MIX_WET_GARNISH_SCALE = 0.20f;
static const float MIX_GRAIN_A_GARNISH   = 0.20f;
static const float MIX_GRAIN_A_FB        = 0.10f;
static const float MIX_DEEP_OUTPUT_SCALE = 0.08f;
static const float MIX_DEEP_GOVERN_SCALE = 0.14f;
// Stereo: tap B and tap C lean to opposite sides, everything else centre.
// L + R still sums to the mono signal, so a mono rig loses nothing.
static const float STEREO_SPREAD         = 0.60f;
// Mix law: the dry stays at full until MIX_DRY_HOLD, then fades out by the top
// of the knob. A straight dry/wet crossfade took 13 dB off the pick attack by
// the time the wet was loud enough to hear.
static const float MIX_DRY_HOLD          = 0.60f;

static const float WRITE_DRY_BLEND_BASE  = 0.25f;
static const float WRITE_DRY_BLEND_SCALE = 0.45f;

static const float FB_MAX_SAFE           = 0.95f;
static const float FB_GLOBAL_SCALE       = 0.95f; // safe now the loop is limited
static const float FB_LOWPASS_COEF       = 0.30f; // ~2.6 kHz
// Darkening is mixed in by the feedback knob rather than always in circuit:
// low feedback keeps its top end, high feedback rolls off so the loop does not
// pile up brightness as it holds.
static const float FB_TONE_MIX           = 0.85f;
// Loop limiter. Lets FB_GLOBAL_SCALE run high enough to hold without the
// softclip being the only thing standing between a repeat and a runaway.
static const float FB_LIMIT_CEILING      = 0.55f;
static const float FB_LIMIT_ATT_MS       = 1.5f;
static const float FB_LIMIT_REL_MS       = 180.f;

static const float BODY_RESONIN_OCTAVE_SCALE   = 0.85f;
static const float BODY_RESONIN_GARNISH_SCALE  = 0.60f;
static const float BODY_RESONIN_TAPREAD_SCALE  = 0.18f;
static const float BODY_FB_INPUT_SCALE         = 0.08f;

static const float SWELL_PREDELAY_SECS     = 0.0032f;
static const float SWELL_ATTACK_COEF       = 0.18f;
static const float SWELL_RELEASE_COEF      = 0.0035f;
static const float SWELL_TRIGGER_BASE      = 0.0010f;
static const float SWELL_TRIGGER_ENV_SCALE = 0.030f;
static const float SWELL_HOLD_SECS         = 0.004f;
static const float SWELL_COOLDOWN_SECS     = 0.060f;
static const float SWELL_SLEW_DOWN_SECS    = 0.0015f;
static const float SWELL_SLEW_UP_SECS      = 0.055f;
static const int   SWELL_BUFFER_LEN        = 256;
// The swell predelays the dry path. Without the same delay on the wet the two
// comb against each other whenever the switch is on.
static const int   WET_ALIGN_LEN           = 256;

static const float KNOB_SLEW_COEF          = 0.05f;
static const int   POT_BITS                = 12;
static const float POT_MAX                 = 4095.f;
static const uint32_t BUTTON_DEBOUNCE_MS   = 18;
static const uint32_t BUTTON_HOLD_TIME_MS  = 275;

static const int PIN_SW_BLOOM    = D7;
static const int PIN_SW_BODY     = D8;
static const int PIN_SW_RETRACE  = D9;
static const int PIN_SW_SWELL    = D10;
static const int PIN_BTN_BYPASS  = D25;
static const int PIN_LED_BYPASS  = D22;
static const int PIN_BTN_GEOHOLD = D26;
static const int PIN_LED_GEOHOLD = D23;

// LED drive. Bypass LED now lights when the effect is RUNNING, not when it is
// bypassed. LED_ACTIVE_LOW flips the electrical polarity if the LEDs are wired
// to the rail rather than to ground.
static const bool LED_ACTIVE_LOW = false;

static inline void ledWrite(int pin, bool on)
{
  digitalWrite(pin, (on != LED_ACTIVE_LOW) ? HIGH : LOW);
}

static inline float clampf(float x, float lo, float hi)
{
  if(x < lo) return lo;
  if(x > hi) return hi;
  return x;
}

static inline float lerpf(float a, float b, float t)
{
  return a + (b - a) * t;
}

static inline float smoothstepf(float edge0, float edge1, float x)
{
  x = clampf((x - edge0) / (edge1 - edge0), 0.f, 1.f);
  return x * x * (3.f - 2.f * x);
}

static inline float softclipf(float x)
{
  return x / (1.f + fabsf(x));
}

static float sampleRate = 48000.f;

static float DSY_SDRAM_BSS delayBuffer[DELAY_BUFFER_LEN];
static int delayWritePos = 0;

static inline float delayReadInterp(float readPos)
{
  while(readPos < 0.f)                     readPos += (float)DELAY_BUFFER_LEN;
  while(readPos >= (float)DELAY_BUFFER_LEN) readPos -= (float)DELAY_BUFFER_LEN;

  int   i0   = (int)readPos;
  int   i1   = (i0 + 1 >= DELAY_BUFFER_LEN) ? 0 : i0 + 1;
  float frac = readPos - (float)i0;
  return delayBuffer[i0] + (delayBuffer[i1] - delayBuffer[i0]) * frac;
}

struct LSystemOut
{
  bool     spawn;
  bool     wrapped;
  uint8_t  tap;
  float    pitch;
  uint8_t  geomCode;
  char     symbol;
  int8_t   heading;
  int16_t  x;
  int16_t  y;
  int16_t  radius2;
  float    turnness;
  uint16_t bboxW;
  uint16_t bboxH;
  float    bboxAspect;
  float    selfNear;
  float    cellDensity;
  float    spreadN;
  float    quadXBias;
  float    quadYBias;
  float    centroidXBias;
  float    centroidYBias;
  float    frontierN;
};

static char lsysBuf1[LSYS_BUFFER_LEN];
static char lsysBuf2[LSYS_BUFFER_LEN];
static char* lsystemStr   = lsysBuf1;
static int   lsystemLen   = 0;
static int   lsystemPos   = 0;

static void lsystemGenerate(int generations)
{
  lsysBuf1[0] = 'F';
  lsysBuf1[1] = '\0';

  char* cur  = lsysBuf1;
  char* next = lsysBuf2;

  for(int gen = 0; gen < generations; gen++)
  {
    int rIdx = 0;
    int wIdx = 0;

    while(cur[rIdx] != '\0' && wIdx < LSYS_BUFFER_LEN - 5)
    {
      char c = cur[rIdx++];
      if(c == 'F')      { next[wIdx++] = 'F'; next[wIdx++] = '+'; next[wIdx++] = 'G'; }
      else if(c == 'G') { next[wIdx++] = 'F'; next[wIdx++] = '-'; next[wIdx++] = 'G'; }
      else              { next[wIdx++] = c; }
    }
    next[wIdx] = '\0';

    char* tmp = cur; cur = next; next = tmp;
  }

  lsystemStr = cur;
  lsystemLen = strlen(lsystemStr);
  lsystemPos = 0;
}

static int   turtleHeading    = 0;
static int   turtleX          = 0;
static int   turtleY          = 0;
static int   turtleMinX       = 0;
static int   turtleMaxX       = 0;
static int   turtleMinY       = 0;
static int   turtleMaxY       = 0;
static int   turtleStepCount  = 0;
static float turtleTurnDensity = 0.f;
static float turtleSelfNear   = 0.f;

static int turtleHistX[TURTLE_HISTORY_LEN];
static int turtleHistY[TURTLE_HISTORY_LEN];
static int turtleHistHead  = 0;
static int turtleHistCount = 0;

static uint8_t spatialVisitGrid[SPATIAL_GRID_SIZE][SPATIAL_GRID_SIZE];
static int     spatialOccupiedCells = 0;
static int     spatialQuadCount[4]  = {0, 0, 0, 0};
static int     spatialVisitCount    = 0;
static float   spatialVisitSumX     = 0.f;
static float   spatialVisitSumY     = 0.f;

static float cycleMemGrid[SPATIAL_GRID_SIZE][SPATIAL_GRID_SIZE];
// Running totals kept in step with the grid, so the spread and bias figures
// do not need a 256-cell scan on every forward step.
static float cycleMemTotal     = 0.f;
static float cycleMemRightSum  = 0.f;
static float cycleMemUpSum     = 0.f;
static int   cycleMemActive    = 0;
static float cycleMemNear   = 0.f;
static float cycleMemSpread = 0.f;
static float cycleMemXBias  = 0.f;
static float cycleMemYBias  = 0.f;

static inline void turtleHistoryPush(int x, int y)
{
  turtleHistX[turtleHistHead] = x;
  turtleHistY[turtleHistHead] = y;
  turtleHistHead = (turtleHistHead + 1) % TURTLE_HISTORY_LEN;
  if(turtleHistCount < TURTLE_HISTORY_LEN) turtleHistCount++;
}

static inline float turtleComputeSelfNear(int x, int y)
{
  if(turtleHistCount <= 1) return SELF_NEAR_SCORE_DISTANT;

  int best = 9999;
  for(int i = 0; i < turtleHistCount; i++)
  {
    int dist = abs(x - turtleHistX[i]) + abs(y - turtleHistY[i]);
    if(dist < best) best = dist;
  }

  if(best <= 1) return SELF_NEAR_SCORE_DIST_1;
  if(best == 2) return SELF_NEAR_SCORE_DIST_2;
  if(best == 3) return SELF_NEAR_SCORE_DIST_3;
  if(best == 4) return SELF_NEAR_SCORE_DIST_4;
  return SELF_NEAR_SCORE_DISTANT;
}

static inline int spatialGridCoord(int latticeVal)
{
  int c = (latticeVal >> SPATIAL_CELL_SHIFT) + (SPATIAL_GRID_SIZE / 2);
  if(c < 0) c = 0;
  if(c >= SPATIAL_GRID_SIZE) c = SPATIAL_GRID_SIZE - 1;
  return c;
}

static inline void spatialRecordVisit(int x, int y)
{
  int gx = spatialGridCoord(x);
  int gy = spatialGridCoord(y);

  if(spatialVisitGrid[gx][gy] == 0) spatialOccupiedCells++;
  if(spatialVisitGrid[gx][gy] < 255) spatialVisitGrid[gx][gy]++;

  float& cellHeat = cycleMemGrid[gx][gy];
  float  heatWas  = cellHeat;
  cellHeat += CYCLE_MEM_VISIT_ACCUM_RATE * (1.f - cellHeat);
  if(cellHeat > 1.f) cellHeat = 1.f;

  float heatDelta = cellHeat - heatWas;
  cycleMemTotal += heatDelta;
  if(gx >= SPATIAL_GRID_SIZE / 2) cycleMemRightSum += heatDelta;
  if(gy >= SPATIAL_GRID_SIZE / 2) cycleMemUpSum    += heatDelta;
  if(heatWas <= CYCLE_MEM_ACTIVE_THRESHOLD && cellHeat > CYCLE_MEM_ACTIVE_THRESHOLD)
    cycleMemActive++;

  int qx = (x >= 0) ? 1 : 0;
  int qy = (y >= 0) ? 1 : 0;
  spatialQuadCount[qx | (qy << 1)]++;

  spatialVisitCount++;
  spatialVisitSumX += (float)x;
  spatialVisitSumY += (float)y;
}

static inline float spatialComputeCellDensity(int x, int y)
{
  int c = spatialVisitGrid[spatialGridCoord(x)][spatialGridCoord(y)];
  return clampf(((float)c - 1.f) / CELL_DENSITY_SATURATION_VISITS, 0.f, 1.f);
}

static inline float spatialComputeSpreadN()
{
  int denom = turtleStepCount + 1;
  int maxCells = SPATIAL_GRID_SIZE * SPATIAL_GRID_SIZE;
  if(denom > maxCells) denom = maxCells;
  if(denom < 1) denom = 1;
  return clampf((float)spatialOccupiedCells / (float)denom, 0.f, 1.f);
}

static inline float spatialComputeQuadXBias()
{
  int left  = spatialQuadCount[0] + spatialQuadCount[2];
  int right = spatialQuadCount[1] + spatialQuadCount[3];
  int total = left + right;
  if(total <= 0) return 0.f;
  return clampf((float)(right - left) / (float)total, -1.f, 1.f);
}

static inline float spatialComputeQuadYBias()
{
  int down  = spatialQuadCount[0] + spatialQuadCount[1];
  int up    = spatialQuadCount[2] + spatialQuadCount[3];
  int total = down + up;
  if(total <= 0) return 0.f;
  return clampf((float)(up - down) / (float)total, -1.f, 1.f);
}

static inline float spatialComputeCentroidXBias(int x)
{
  if(spatialVisitCount <= 0) return 0.f;
  float meanX = spatialVisitSumX / (float)spatialVisitCount;
  float halfW = 0.5f * (float)(turtleMaxX - turtleMinX + 1);
  if(halfW < 1.f) halfW = 1.f;
  return clampf(((float)x - meanX) / (halfW + 0.5f), -1.f, 1.f);
}

static inline float spatialComputeCentroidYBias(int y)
{
  if(spatialVisitCount <= 0) return 0.f;
  float meanY = spatialVisitSumY / (float)spatialVisitCount;
  float halfH = 0.5f * (float)(turtleMaxY - turtleMinY + 1);
  if(halfH < 1.f) halfH = 1.f;
  return clampf(((float)y - meanY) / (halfH + 0.5f), -1.f, 1.f);
}

static inline float spatialComputeFrontierN(int x, int y)
{
  int dMin = turtleMaxX - turtleMinX + 1;
  int dArr[4] = { x - turtleMinX, turtleMaxX - x, y - turtleMinY, turtleMaxY - y };
  for(int i = 0; i < 4; i++) if(dArr[i] < dMin) dMin = dArr[i];

  int bbW   = turtleMaxX - turtleMinX + 1;
  int bbH   = turtleMaxY - turtleMinY + 1;
  float halfMin = 0.5f * (float)(bbW < bbH ? bbW : bbH);
  if(halfMin < 1.f) halfMin = 1.f;
  return clampf(1.f - ((float)dMin / (halfMin + 0.5f)), 0.f, 1.f);
}

static inline float cycleMemComputeNear(int x, int y)
{
  int gx = spatialGridCoord(x);
  int gy = spatialGridCoord(y);
  float best = 0.f;
  for(int ox = -1; ox <= 1; ox++)
  {
    int ix = gx + ox;
    if(ix < 0 || ix >= SPATIAL_GRID_SIZE) continue;
    for(int oy = -1; oy <= 1; oy++)
    {
      int iy = gy + oy;
      if(iy < 0 || iy >= SPATIAL_GRID_SIZE) continue;
      float w = cycleMemGrid[ix][iy];
      if(ox != 0 || oy != 0) w *= CYCLE_MEM_NEIGHBOR_DECAY;
      if(w > best) best = w;
    }
  }
  return clampf(best, 0.f, 1.f);
}

static inline float cycleMemComputeSpreadN()
{
  return clampf((float)cycleMemActive / SPATIAL_SPREAD_NORM_CELLS, 0.f, 1.f);
}

static inline float cycleMemComputeXBias()
{
  if(cycleMemTotal <= 1e-6f) return 0.f;
  float right = cycleMemRightSum;
  float left  = cycleMemTotal - right;
  return clampf((right - left) / cycleMemTotal, -1.f, 1.f);
}

static inline float cycleMemComputeYBias()
{
  if(cycleMemTotal <= 1e-6f) return 0.f;
  float up   = cycleMemUpSum;
  float down = cycleMemTotal - up;
  return clampf((up - down) / cycleMemTotal, -1.f, 1.f);
}

// Decay runs once per cycle wrap, so rebuilding the totals here is cheap.
static void cycleMemDecay(float decayFactor)
{
  cycleMemTotal = cycleMemRightSum = cycleMemUpSum = 0.f;
  cycleMemActive = 0;
  for(int ix = 0; ix < SPATIAL_GRID_SIZE; ix++)
    for(int iy = 0; iy < SPATIAL_GRID_SIZE; iy++)
    {
      float h = (cycleMemGrid[ix][iy] *= decayFactor);
      cycleMemTotal += h;
      if(ix >= SPATIAL_GRID_SIZE / 2) cycleMemRightSum += h;
      if(iy >= SPATIAL_GRID_SIZE / 2) cycleMemUpSum    += h;
      if(h > CYCLE_MEM_ACTIVE_THRESHOLD) cycleMemActive++;
    }
}

static void turtleReset(bool clearPersistent = false)
{
  turtleHeading    = 0;
  turtleX          = 0;
  turtleY          = 0;
  turtleMinX = turtleMaxX = 0;
  turtleMinY = turtleMaxY = 0;
  turtleStepCount  = 0;
  turtleTurnDensity = 0.f;
  turtleSelfNear   = 0.f;
  turtleHistCount  = 0;
  turtleHistHead   = 0;
  spatialOccupiedCells = 0;
  spatialVisitCount    = 0;
  spatialVisitSumX     = 0.f;
  spatialVisitSumY     = 0.f;
  memset(spatialVisitGrid, 0, sizeof(spatialVisitGrid));
  for(int i = 0; i < 4; i++) spatialQuadCount[i] = 0;
  turtleHistoryPush(0, 0);
  spatialRecordVisit(0, 0);

  if(clearPersistent)
  {
    memset(cycleMemGrid, 0, sizeof(cycleMemGrid));
    cycleMemTotal = cycleMemRightSum = cycleMemUpSum = 0.f;
    cycleMemActive = 0;
    cycleMemNear   = 0.f;
    cycleMemSpread = 0.f;
    cycleMemXBias  = 0.f;
    cycleMemYBias  = 0.f;
  }
}

static LSystemOut turtleStep(float pitchAmount)
{
  LSystemOut out    = {};
  out.spawn         = true;
  out.pitch         = PITCH_RATIO_UNISON;
  out.bboxAspect    = 1.f;
  out.symbol        = '?';

  if(lsystemLen == 0)
  {
    out.symbol = 'F';
    return out;
  }

  int  pos     = lsystemPos;
  char symbol  = lsystemStr[pos];
  bool wrapped = (pos >= lsystemLen - 1);
  out.symbol   = symbol;

  auto moveForward = [&]()
  {
    switch(turtleHeading & 3)
    {
      case 0: turtleX += 1; break;
      case 1: turtleY += 1; break;
      case 2: turtleX -= 1; break;
      default: turtleY -= 1; break;
    }
    if(turtleX < turtleMinX) turtleMinX = turtleX;
    if(turtleX > turtleMaxX) turtleMaxX = turtleX;
    if(turtleY < turtleMinY) turtleMinY = turtleY;
    if(turtleY > turtleMaxY) turtleMaxY = turtleY;
    turtleStepCount++;

    float nearNow  = turtleComputeSelfNear(turtleX, turtleY);
    turtleSelfNear += SELF_NEAR_LERP_RATE * (nearNow - turtleSelfNear);
    turtleHistoryPush(turtleX, turtleY);
    spatialRecordVisit(turtleX, turtleY);

    cycleMemNear   += CYCLE_MEM_NEAR_LERP_RATE   * (cycleMemComputeNear(turtleX, turtleY) - cycleMemNear);
    cycleMemSpread += CYCLE_MEM_SPREAD_LERP_RATE  * (cycleMemComputeSpreadN()              - cycleMemSpread);
    cycleMemXBias  += CYCLE_MEM_BIAS_LERP_RATE    * (cycleMemComputeXBias()                - cycleMemXBias);
    cycleMemYBias  += CYCLE_MEM_BIAS_LERP_RATE    * (cycleMemComputeYBias()                - cycleMemYBias);

    turtleTurnDensity *= TURN_DENSITY_PER_STEP_DECAY;
  };

  switch(symbol)
  {
    case 'F':
      out.pitch = PITCH_RATIO_UNISON;
      moveForward();
      break;

    case 'G':
      out.pitch = PITCH_RATIO_UNISON;
      moveForward();
      break;

    case '+':
      turtleHeading     = (turtleHeading + 1) & 3;
      turtleTurnDensity = clampf(turtleTurnDensity * TURN_DENSITY_ON_TURN_DECAY + TURN_DENSITY_PER_TURN_GAIN, 0.f, TURN_DENSITY_MAX);
      turtleSelfNear   *= SELF_NEAR_ON_TURN_DECAY;
      out.tap           = 2;
      break;

    case '-':
      turtleHeading     = (turtleHeading + 3) & 3;
      turtleTurnDensity = clampf(turtleTurnDensity * TURN_DENSITY_ON_TURN_DECAY + TURN_DENSITY_PER_TURN_GAIN, 0.f, TURN_DENSITY_MAX);
      turtleSelfNear   *= SELF_NEAR_ON_TURN_DECAY;
      out.tap           = 2;
      break;

    default:
      out.spawn = false;
      break;
  }

  uint8_t geomCode = (uint8_t)(((turtleHeading & 3) << 1) | ((abs(turtleX) ^ abs(turtleY)) & 1));

  out.wrapped       = wrapped;
  out.geomCode      = geomCode;
  out.heading       = (int8_t)(turtleHeading & 3);
  out.x             = (int16_t)turtleX;
  out.y             = (int16_t)turtleY;
  out.radius2       = (int16_t)clampf((float)(turtleX * turtleX + turtleY * turtleY), 0.f, 32767.f);
  out.turnness      = clampf(turtleTurnDensity * TURN_DENSITY_DISPLAY_SCALE, 0.f, 1.f);
  out.bboxW         = (uint16_t)(turtleMaxX - turtleMinX + 1);
  out.bboxH         = (uint16_t)(turtleMaxY - turtleMinY + 1);
  out.bboxAspect    = clampf((float)out.bboxW / (float)(out.bboxH > 0 ? out.bboxH : 1), 0.25f, 4.f);
  out.selfNear      = clampf(turtleSelfNear * BLEND_SELF_NEAR_INSTANT + cycleMemNear   * BLEND_SELF_NEAR_MEMORY, 0.f, 1.f);
  out.cellDensity   = spatialComputeCellDensity(turtleX, turtleY);
  out.spreadN       = clampf(spatialComputeSpreadN() * BLEND_SPREAD_INSTANT    + cycleMemSpread * BLEND_SPREAD_MEMORY,   0.f, 1.f);
  out.quadXBias     = clampf(spatialComputeQuadXBias() * BLEND_QUAD_BIAS_INSTANT + cycleMemXBias * BLEND_QUAD_BIAS_MEMORY, -1.f, 1.f);
  out.quadYBias     = clampf(spatialComputeQuadYBias() * BLEND_QUAD_BIAS_INSTANT + cycleMemYBias * BLEND_QUAD_BIAS_MEMORY, -1.f, 1.f);
  out.centroidXBias = spatialComputeCentroidXBias(turtleX);
  out.centroidYBias = spatialComputeCentroidYBias(turtleY);
  out.frontierN     = spatialComputeFrontierN(turtleX, turtleY);

  if(symbol == 'F') out.tap = 0;
  else if(symbol == 'G') out.tap = 1;

  if(symbol == '+' || symbol == '-')
    out.pitch = lerpf(PITCH_RATIO_UNISON, PITCH_LUT[geomCode & 7], pitchAmount);

  lsystemPos = wrapped ? 0 : (pos + 1);

  return out;
}

struct InputShaper
{
  float drive;
  float asymBody;
  float asymEdge;
  float stage1;
  float stage2;
  float bias;
  float phase;
  float phaseInc;
  float modDepth;
  float evenMix;
  float oddMix;
  float bite;
  float userDriveMul;
  float presence;      // input-presence envelope; keeps the bias from idling
  float dcBlockX1;
  float dcBlockY1;
  float dcBlockCoef;
  // live values: glide per sample toward the per-slice settings above
  float lDrive, lAsymBody, lAsymEdge, lStage1, lStage2, lBias;
  float lModDepth, lEvenMix, lOddMix, lBite, lUserDriveMul;
  float glideK;

  void reset()
  {
    drive        = 1.f;
    asymBody     = 0.f;
    asymEdge     = 0.f;
    stage1       = 0.f;
    stage2       = 0.f;
    bias         = 0.f;
    phase        = 0.f;
    phaseInc     = 0.f;
    modDepth     = 0.f;
    evenMix      = 0.f;
    oddMix       = 0.f;
    bite         = 0.f;
    userDriveMul = 1.f;
    presence     = 0.f;
    dcBlockX1    = 0.f;
    dcBlockY1    = 0.f;
    dcBlockCoef  = expf(-2.f * 3.14159265f * DC_BLOCKER_CUTOFF_HZ / sampleRate);
    lDrive = 1.f; lAsymBody = lAsymEdge = lStage1 = lStage2 = lBias = 0.f;
    lModDepth = lEvenMix = lOddMix = lBite = 0.f; lUserDriveMul = 1.f;
    glideK = 1.f - expf(-1.f / (SHAPER_GLIDE_MS * 0.001f * sampleRate));
  }

  void update(int geomCode, int radius2, float turnness, float bboxAspect, float selfNear,
              float cellDensity, float spreadN, float quadXBias, float quadYBias,
              float centroidXBias, float centroidYBias, float frontierN,
              float govern, float driveCtl, float sr)
  {
    float amt   = clampf(govern,   0.f, 1.f);
    float dctl  = clampf(driveCtl, 0.f, 1.f);
    float tUser = 1.f + SHAPER_DRIVE_USER_SCALE * dctl;

    if(amt < 0.001f && dctl < 0.001f)
    {
      drive = 1.f; asymBody = 0.f; asymEdge = 0.f;
      stage1 = 0.f; stage2 = 0.f; bias = 0.f;
      modDepth = 0.f; evenMix = 0.f; oddMix = 0.f;
      bite = 0.f; phaseInc = 0.f; userDriveMul = tUser;
      return;
    }

    uint8_t g        = (uint8_t)(geomCode & 7);
    int heading      = (g >> 1) & 3;
    bool latticeFlip = (g & 1) != 0;
    float radiusN    = clampf(sqrtf((float)radius2) * 0.10f, 0.f, 1.f);
    float turnN      = clampf(turnness,      0.f, 1.f);
    float nearN      = clampf(selfNear,      0.f, 1.f);
    float cellN      = clampf(cellDensity,   0.f, 1.f);
    float spread     = clampf(spreadN,       0.f, 1.f);
    float qx         = clampf(quadXBias,    -1.f, 1.f);
    float qy         = clampf(quadYBias,    -1.f, 1.f);
    float cx         = clampf(centroidXBias,-1.f, 1.f);
    float cy         = clampf(centroidYBias,-1.f, 1.f);
    float frontier   = clampf(frontierN,     0.f, 1.f);
    float aspectTilt = clampf(fabsf(logf(clampf(bboxAspect, 0.25f, 4.f)) / 0.69314718f) * 0.60f, 0.f, 1.f);
    bool  turnHot    = (turnN > 0.35f) || (nearN > 0.45f && frontier > 0.35f);
    float dirPol     = ((heading == 2 || heading == 3) ^ latticeFlip) ? -1.f : 1.f;
    dirPol = clampf(dirPol + 0.22f * qx - 0.12f * qy + 0.28f * cx - 0.18f * cy, -1.f, 1.f);

    float tDrive    = 1.f + amt * (0.18f + 0.08f * radiusN + 0.05f * turnN + 0.05f * nearN + 0.04f * cellN + 0.03f * spread + 0.04f * frontier);
    float tAsymBody = dirPol * amt * (0.05f + 0.08f * radiusN + 0.04f * turnN + 0.03f * aspectTilt + 0.05f * cellN + 0.02f * spread + 0.04f * fabsf(cx));
    float tAsymEdge = -dirPol * amt * (0.03f + 0.07f * turnN + 0.05f * nearN + 0.04f * cellN + 0.03f * ((heading == 1 || heading == 3) ? 1.f : 0.f) + 0.06f * frontier + 0.03f * fabsf(cy));
    float tStage1   = amt * (0.10f + 0.10f * radiusN + 0.04f * turnN + 0.04f * aspectTilt + 0.04f * spread + 0.03f * fabsf(cx));
    float tStage2   = amt * (0.08f + 0.09f * turnN + 0.05f * nearN + 0.05f * cellN + 0.04f * latticeFlip + 0.07f * frontier + 0.03f * fabsf(cy));
    float tBias     = dirPol * amt * (0.002f + 0.006f * turnN + 0.004f * radiusN + 0.002f * aspectTilt + 0.003f * qx + 0.004f * cx);
    float rateHz    = 0.04f + amt * (0.05f + 0.08f * turnN + 0.03f * radiusN + 0.02f * nearN + 0.02f * frontier);
    if(turnHot) rateHz *= 1.08f;
    float tModDepth = amt * (0.004f + 0.008f * turnN + 0.003f * radiusN + 0.002f * nearN + 0.003f * cellN + 0.002f * frontier);
    float tEvenMix  = amt * (0.03f + 0.07f * ((heading == 0 || heading == 2) ? 1.f : 0.f) + 0.03f * latticeFlip + 0.03f * aspectTilt + 0.03f * spread + 0.02f * fabsf(qx) + 0.03f * fabsf(cx));
    float tOddMix   = amt * (0.04f + 0.06f * ((heading == 1 || heading == 3) ? 1.f : 0.f) + 0.05f * turnN + 0.05f * nearN + 0.04f * cellN + 0.02f * fabsf(qy) + 0.04f * frontier + 0.03f * fabsf(cy));
    float tBite     = amt * (0.03f + 0.08f * turnN + 0.02f * radiusN + 0.04f * nearN + 0.03f * cellN + 0.02f * aspectTilt + 0.05f * frontier);
    if(turnHot) { tOddMix += 0.02f * amt; tStage2 += 0.02f * amt; tBite += 0.01f * amt; }

    tStage1 += 0.03f * dctl; tStage2 += 0.05f * dctl;
    tOddMix += 0.03f * dctl; tBite   += 0.03f * dctl;

    tAsymBody = clampf(tAsymBody, -0.22f, 0.22f);
    tAsymEdge = clampf(tAsymEdge, -0.18f, 0.18f);
    tStage1   = clampf(tStage1,    0.f,   0.24f);
    tStage2   = clampf(tStage2,    0.f,   0.26f);
    tEvenMix  = clampf(tEvenMix,   0.f,   0.12f);
    tOddMix   = clampf(tOddMix,    0.f,   0.12f);
    tBite     = clampf(tBite,      0.f,   0.12f);

    drive        += SHAPER_PARAM_SLEW * (tDrive    - drive);
    asymBody     += SHAPER_PARAM_SLEW * (tAsymBody - asymBody);
    asymEdge     += SHAPER_PARAM_SLEW * (tAsymEdge - asymEdge);
    stage1       += SHAPER_PARAM_SLEW * (tStage1   - stage1);
    stage2       += SHAPER_PARAM_SLEW * (tStage2   - stage2);
    bias         += SHAPER_PARAM_SLEW * (tBias     - bias);
    modDepth     += SHAPER_PARAM_SLEW * (tModDepth - modDepth);
    evenMix      += SHAPER_PARAM_SLEW * (tEvenMix  - evenMix);
    oddMix       += SHAPER_PARAM_SLEW * (tOddMix   - oddMix);
    bite         += SHAPER_PARAM_SLEW * (tBite     - bite);
    userDriveMul += SHAPER_PARAM_SLEW * (tUser     - userDriveMul);

    phaseInc    = rateHz / sr;
    dcBlockCoef = expf(-2.f * 3.14159265f * DC_BLOCKER_CUTOFF_HZ / sr);
  }

  inline float triLFO()
  {
    phase += phaseInc;
    if(phase >= 1.f) phase -= 1.f;
    float t = (phase < 0.5f) ? phase * 2.f : 2.f - phase * 2.f;
    return t * 2.f - 1.f;
  }

  inline float process(float x)
  {
    lDrive        += glideK * (drive        - lDrive);
    lAsymBody     += glideK * (asymBody     - lAsymBody);
    lAsymEdge     += glideK * (asymEdge     - lAsymEdge);
    lStage1       += glideK * (stage1       - lStage1);
    lStage2       += glideK * (stage2       - lStage2);
    lBias         += glideK * (bias         - lBias);
    lModDepth     += glideK * (modDepth     - lModDepth);
    lEvenMix      += glideK * (evenMix      - lEvenMix);
    lOddMix       += glideK * (oddMix       - lOddMix);
    lBite         += glideK * (bite         - lBite);
    lUserDriveMul += glideK * (userDriveMul - lUserDriveMul);

    // Track input presence so the DC bias term cannot sit in the loop on its
    // own: with silence in and govern up it self-sustained at 0.004 RMS.
    float ax = fabsf(x);
    presence += ((ax > presence) ? 0.01f : 0.0002f) * (ax - presence);
    float presGate = clampf(presence * 80.f, 0.f, 1.f);

    float lfo = (phaseInc > 0.f) ? triLFO() : 0.f;
    x = x * lDrive * lUserDriveMul * (1.f + lModDepth * lfo) + lBias * presGate;

    float a1    = clampf(lAsymBody + 0.10f * lModDepth * lfo, -0.24f, 0.24f);
    float thr1p = clampf((0.88f - 0.42f * lStage1) / (1.f + a1), 0.34f, 0.94f);
    float thr1n = clampf((0.88f - 0.42f * lStage1) / (1.f - a1), 0.34f, 0.94f);

    if(x > thr1p)       { float d = x - thr1p;  x = thr1p + d / (1.f + (2.f + 3.f * lStage1) * d); }
    else if(x < -thr1n) { float d = -x - thr1n; x = -(thr1n + d / (1.f + (2.f + 3.f * lStage1) * d)); }

    float x2 = x * x;
    float odd3 = x * (x2 - 0.75f);
    x += lEvenMix * 0.55f * x * fabsf(x)
       + lOddMix  * (0.75f * odd3 + 0.25f * odd3 * (x2 - 0.55f));

    float a2    = clampf(lAsymEdge - 0.14f * lBite * lfo, -0.22f, 0.22f);
    float thr2p = clampf((0.76f - 0.34f * lStage2 - 0.10f * lBite) / (1.f + a2), 0.24f, 0.86f);
    float thr2n = clampf((0.76f - 0.34f * lStage2 + 0.06f * lBite) / (1.f - a2), 0.24f, 0.86f);

    if(x > thr2p)       { float d = x - thr2p;  x = thr2p + d / (1.f + (3.f + 5.f * lStage2) * d); }
    else if(x < -thr2n) { float d = -x - thr2n; x = -(thr2n + d / (1.f + (3.f + 5.f * lStage2) * d)); }

    x = softclipf(x * (1.f + 0.35f * lBite));

    float y     = dcBlockCoef * (dcBlockY1 + x - dcBlockX1);
    dcBlockX1   = x;
    dcBlockY1   = y;
    return y;
  }
};

static InputShaper inputShaper;

struct ResonantBody
{
  float lpWet1, bpWet1, lpWet2, bpWet2;
  float lpFb1,  bpFb1,  lpFb2,  bpFb2;

  float freq1, freq2;
  float damp1, damp2;
  float wetMix, fbMix;

  float coefWet1, coefWet2;
  float coefFb1,  coefFb2;

  void reset()
  {
    lpWet1 = bpWet1 = lpWet2 = bpWet2 = 0.f;
    lpFb1  = bpFb1  = lpFb2  = bpFb2  = 0.f;
    freq1  = BODY_BASE_FREQ_NORTH;
    freq2  = 1180.f;
    damp1  = 0.18f;
    damp2  = 0.16f;
    wetMix = 0.f;
    fbMix  = 0.f;
    coefWet1 = coefWet2 = coefFb1 = coefFb2 = 0.001f;
  }

  void update(int geomCode, int radius2, float turnness, float bboxAspect, float selfNear,
              float cellDensity, float spreadN, float quadXBias, float quadYBias,
              float centroidXBias, float centroidYBias, float frontierN,
              float govern, bool enabled)
  {
    float amt = clampf(govern, 0.f, 1.f);
    int heading      = (geomCode >> 1) & 3;
    bool latticeFlip = (geomCode & 1) != 0;
    float radiusN    = clampf(sqrtf((float)radius2) * 0.10f, 0.f, 1.f);
    float turnN      = clampf(turnness,      0.f, 1.f);
    float nearN      = clampf(selfNear,      0.f, 1.f);
    float cellN      = clampf(cellDensity,   0.f, 1.f);
    float spread     = clampf(spreadN,       0.f, 1.f);
    float qx         = clampf(quadXBias,    -1.f, 1.f);
    float qy         = clampf(quadYBias,    -1.f, 1.f);
    float cx         = clampf(centroidXBias,-1.f, 1.f);
    float cy         = clampf(centroidYBias,-1.f, 1.f);
    float frontier   = clampf(frontierN,     0.f, 1.f);
    float aspectSkew = clampf((clampf(bboxAspect, 0.25f, 4.f) - 1.f) * 0.50f, -1.f, 1.f);
    bool  turnHot    = (turnN > 0.35f) || (nearN > 0.45f && frontier > 0.35f);

    float tFreq1 = BODY_HEADING_FREQS[heading];
    tFreq1 *= lerpf(BODY_FREQ_RADIUS_SCALE_LO, BODY_FREQ_RADIUS_SCALE_HI, radiusN);
    tFreq1 *= (1.f + 0.10f * aspectSkew + 0.06f * nearN + 0.04f * qx - 0.03f * qy + 0.07f * cx - 0.05f * cy + 0.08f * frontier);
    if(latticeFlip) tFreq1 *= 1.10f;
    if(turnHot)     tFreq1 *= 1.08f;

    float spacing = 1.35f + 0.50f * turnN + 0.28f * nearN + 0.20f * fabsf(aspectSkew) + 0.18f * cellN + 0.10f * spread + 0.12f * frontier + 0.10f * fabsf(cx) + 0.06f * fabsf(cy) + (latticeFlip ? 0.18f : 0.f);
    float tFreq2  = clampf(tFreq1 * spacing, BODY_FREQ2_CLAMP_MIN, BODY_FREQ2_CLAMP_MAX);
    tFreq1        = clampf(tFreq1, BODY_FREQ_CLAMP_MIN, BODY_FREQ_CLAMP_MAX);

    float tDamp1   = lerpf(0.24f, 0.16f - 0.06f * turnN - 0.03f * nearN - 0.02f * cellN, amt);
    float tDamp2   = lerpf(0.22f, 0.15f - 0.06f * turnN - 0.03f * nearN - 0.02f * cellN, amt);
    float tWetMix  = enabled ? lerpf(0.05f, 0.22f + 0.12f * turnN + 0.08f * radiusN + 0.10f * nearN + 0.08f * cellN + 0.05f * spread + 0.05f * fabsf(aspectSkew) + 0.08f * frontier + 0.06f * fabsf(cx), amt) : 0.f;
    float tFbMix   = enabled ? lerpf(0.00f, 0.06f + 0.05f * turnN + 0.03f * radiusN + 0.05f * nearN + 0.04f * cellN + 0.04f * frontier + 0.03f * fabsf(cy), amt) : 0.f;

    freq1  += BODY_PARAM_SLEW * (tFreq1 - freq1);
    freq2  += BODY_PARAM_SLEW * (tFreq2 - freq2);
    damp1  += BODY_PARAM_SLEW * (tDamp1 - damp1);
    damp2  += BODY_PARAM_SLEW * (tDamp2 - damp2);
    wetMix += BODY_PARAM_SLEW * (tWetMix - wetMix);
    fbMix  += BODY_PARAM_SLEW * (tFbMix  - fbMix);

    float sr = sampleRate;
    coefWet1 = clampf(2.f * sinf(3.14159265f * clampf(freq1,         20.f, 0.24f * sr) / sr), 0.001f, 0.99f);
    coefWet2 = clampf(2.f * sinf(3.14159265f * clampf(freq2,         20.f, 0.24f * sr) / sr), 0.001f, 0.99f);
    coefFb1  = clampf(2.f * sinf(3.14159265f * clampf(freq1 * 0.94f, 20.f, 0.24f * sr) / sr), 0.001f, 0.99f);
    coefFb2  = clampf(2.f * sinf(3.14159265f * clampf(freq2 * 0.90f, 20.f, 0.24f * sr) / sr), 0.001f, 0.99f);
  }

  inline float stepBandpass(float x, float& lp, float& bp, float coef, float damp)
  {
    float q = clampf(damp, 0.04f, 0.35f);
    lp += coef * bp;
    float hp = x - lp - q * bp;
    bp += coef * hp;
    return bp;
  }

  inline float processWet(float x)
  {
    return 0.72f * stepBandpass(x, lpWet1, bpWet1, coefWet1, damp1)
         + 0.48f * stepBandpass(x, lpWet2, bpWet2, coefWet2, damp2);
  }

  inline float processFeedback(float x)
  {
    return 0.65f * stepBandpass(x, lpFb1, bpFb1, coefFb1, damp1 + 0.03f)
         + 0.38f * stepBandpass(x, lpFb2, bpFb2, coefFb2, damp2 + 0.03f);
  }
};

static ResonantBody resonantBody;

struct Grain
{
  bool  active;
  float readPos;
  float rateInc;
  int   age;
  int   length;
  float amplitude;
};

static const int TAP_COUNT          = 3;
static const int GRAINS_PER_TAP     = 8;
static const int DEEP_GRAIN_COUNT   = 10;

static Grain tapGrains[TAP_COUNT][GRAINS_PER_TAP];
static Grain deepGrains[DEEP_GRAIN_COUNT];

static inline float grainTriWindow(int age, int length)
{
  if(length <= 1) return 0.f;
  float phase = (float)age / (float)(length - 1);
  return clampf(1.f - 2.f * fabsf(phase - 0.5f), 0.f, 1.f);
}

static void grainsResetAll()
{
  for(int t = 0; t < TAP_COUNT; t++)
    for(int i = 0; i < GRAINS_PER_TAP; i++)
      tapGrains[t][i] = {false, 0.f, 0.f, 0, 0, 0.f};
  for(int i = 0; i < DEEP_GRAIN_COUNT; i++)
    deepGrains[i] = {false, 0.f, 0.f, 0, 0, 0.f};
}

static bool grainSpawnOnTap(int tap, float anchorPos, float rate, int lengthSamps, float amplitude)
{
  for(int i = 0; i < GRAINS_PER_TAP; i++)
  {
    if(!tapGrains[tap][i].active)
    {
      Grain& g   = tapGrains[tap][i];
      g.active    = true;
      g.age       = 0;
      g.length    = lengthSamps;
      g.amplitude = amplitude;
      float r     = fabsf(rate);
      g.readPos   = (rate >= 0.f) ? anchorPos - (float)lengthSamps : anchorPos;
      g.rateInc   = (rate >= 0.f) ? r : -r;
      return true;
    }
  }
  return false;
}

static bool grainSpawnDeep(float anchorPos, float rate, int lengthSamps, float amplitude)
{
  for(int i = 0; i < DEEP_GRAIN_COUNT; i++)
  {
    if(!deepGrains[i].active)
    {
      Grain& g   = deepGrains[i];
      g.active    = true;
      g.age       = 0;
      g.length    = lengthSamps;
      g.amplitude = amplitude;
      float r     = fabsf(rate);
      g.readPos   = (rate >= 0.f) ? anchorPos - (float)lengthSamps : anchorPos;
      g.rateInc   = (rate >= 0.f) ? r : -r;
      return true;
    }
  }
  return false;
}

static inline float grainsProcess(Grain* grainPool, int poolSize)
{
  float sum = 0.f;
  for(int i = 0; i < poolSize; i++)
  {
    if(!grainPool[i].active) continue;

    Grain& g = grainPool[i];
    sum += delayReadInterp(g.readPos) * grainTriWindow(g.age, g.length) * g.amplitude;

    g.readPos += g.rateInc;
    g.age++;
    if(g.age >= g.length) g.active = false;
  }
  return sum;
}

struct FastSwell
{
  float ringBuf[SWELL_BUFFER_LEN];
  int   writeIdx;
  int   delaySamps;
  float env;
  float prevEnv;
  float gain;
  float targetGain;
  int   holdCounter;
  int   cooldownCounter;
  float slewCoefDown;
  float slewCoefUp;

  void reset(float sr)
  {
    for(int i = 0; i < SWELL_BUFFER_LEN; i++) ringBuf[i] = 0.f;
    writeIdx       = 0;
    delaySamps     = (int)clampf(sr * SWELL_PREDELAY_SECS, 32.f, (float)(SWELL_BUFFER_LEN - 1));
    env            = 0.f;
    prevEnv        = 0.f;
    gain           = 1.f;
    targetGain     = 1.f;
    holdCounter    = 0;
    cooldownCounter = 0;
    slewCoefDown   = 1.f - expf(-1.f / (SWELL_SLEW_DOWN_SECS * sr + 1e-9f));
    slewCoefUp     = 1.f - expf(-1.f / (SWELL_SLEW_UP_SECS   * sr + 1e-9f));
  }

  inline float process(float x, float sr, bool enabled)
  {
    delaySamps = (int)clampf(sr * SWELL_PREDELAY_SECS, 32.f, (float)(SWELL_BUFFER_LEN - 1));
    ringBuf[writeIdx] = x;
    if(++writeIdx >= SWELL_BUFFER_LEN) writeIdx = 0;

    if(!enabled)
    {
      env             = fabsf(x);
      prevEnv         = env;
      gain            = 1.f;
      targetGain      = 1.f;
      holdCounter     = 0;
      cooldownCounter = 0;
      return x;
    }

    int readIdx = writeIdx - delaySamps;
    if(readIdx < 0) readIdx += SWELL_BUFFER_LEN;
    float delayed = ringBuf[readIdx];

    float ax = fabsf(x);
    env += ((ax > env) ? SWELL_ATTACK_COEF : SWELL_RELEASE_COEF) * (ax - env);

    float slope = env - prevEnv;
    prevEnv = env;

    if(cooldownCounter > 0) cooldownCounter--;
    if(holdCounter     > 0) holdCounter--;

    float triggerThreshold = SWELL_TRIGGER_BASE + env * SWELL_TRIGGER_ENV_SCALE;
    if(slope > triggerThreshold && cooldownCounter == 0)
    {
      targetGain      = 0.f;
      holdCounter     = (int)(SWELL_HOLD_SECS     * sr);
      cooldownCounter = (int)(SWELL_COOLDOWN_SECS * sr);
    }

    if(holdCounter == 0) targetGain = 1.f;

    float slewCoef = (targetGain < gain) ? slewCoefDown : slewCoefUp;
    gain += slewCoef * (targetGain - gain);

    return delayed * gain;
  }
};

static FastSwell fastSwell;

static float kTime      = 0.35f;
static float kFeedback  = 0.35f;
static float kMix       = 0.50f;
static float kStructure = 0.50f;
static float kGovern    = 0.50f;
static float kPitchAmt  = 0.20f;
static float kDriveCtl  = 0.00f;
static float gBoostLin  = 1.00f;

static bool swBloom   = false;
static bool swBody    = false;
static bool swRetrace = false;
static bool swSwell   = false;

static bool bypassLatched  = false;
static bool bypassHeld     = false;
static bool bypassActive   = false;
static bool geoHoldLatched = false;
static bool geoHoldHeld    = false;
static bool geoHoldActive  = false;

static int slicePeriodSamps = 200;
static int sliceCounter     = 0;
static int currentLSysGen   = 4;
static int pendingLSysGen   = 4;

static LSystemOut currentGeom;

static float repeatWeight[TAP_COUNT]  = {1.00f, 0.42f, 0.22f};
static float repeatTarget[TAP_COUNT]  = {1.00f, 0.42f, 0.22f};
static float fbWeight[TAP_COUNT]      = {1.00f, 0.26f, 0.10f};
static float fbTarget[TAP_COUNT]      = {1.00f, 0.26f, 0.10f};

static float wetAlignBuf[WET_ALIGN_LEN];
static float wetAlignSideBuf[WET_ALIGN_LEN];
static int   wetAlignIdx = 0;
static float tapModB = 0.f, tapModC = 0.f;
static float tapModTargetB = 0.f, tapModTargetC = 0.f;
static float fbSafetyLP = 0.f;
static float fbLimEnv = 0.f, fbLimGain = 1.f, fbLimAtt = 0.f, fbLimRel = 0.f;
static float tapASmooth = 0.f;      // main tap position, glided (samples)
static float tapTimeGlideK = 0.f;
static const float BYPASS_FADE_STEP = 0.0004f;   // about 5 ms
static float bypassFade = 1.f;

void AudioCallback(float** in, float** out, size_t size)
{
  for(size_t i = 0; i < size; i++)
  {
    float inL    = in[0][i];
    float inR    = in[1][i];
    // Terrarium wires only in[0]. The 0.5 keeps the original gain staging,
    // without picking up whatever sits on the unconnected in[1].
    float inMono = 0.5f * inL;


    float baseMs = lerpf(DELAY_TIME_MIN_MS, DELAY_TIME_MAX_MS, clampf(kTime, 0.f, 1.f));
    int tapA = (int)clampf(baseMs * 0.001f * sampleRate,
                           (float)(DELAY_GUARD_SAMPLES + 512), (float)(DELAY_BUFFER_LEN - 1));
    int tapB = (int)clampf((float)tapA * TAP_B_DELAY_RATIO,
                           (float)(DELAY_GUARD_SAMPLES + 512), (float)(DELAY_BUFFER_LEN - 1));
    int tapC = (int)clampf((float)tapA * TAP_C_DELAY_RATIO,
                           (float)(DELAY_GUARD_SAMPLES + 512), (float)(DELAY_BUFFER_LEN - 1));

    int zone     = (int)clampf(floorf(kStructure * 4.f), 0.f, 3.f);
    int targetGen = STRUCTURE_GEN_TABLE[zone];
    slicePeriodSamps = (int)clampf((float)tapA / (float)STRUCTURE_SLICE_DIV_TABLE[zone],
                                   SLICE_PERIOD_MIN_SAMPS, SLICE_PERIOD_MAX_SAMPS);

    if(++sliceCounter >= slicePeriodSamps)
    {
      sliceCounter = 0;

      if(!geoHoldActive)
      {
        // Regenerating mid-cycle wipes the turtle and clicks (measured a 0.133
        // single-sample step). Wait for the cycle to close, where the geometry
        // restarts anyway.
        if(targetGen != currentLSysGen && pendingLSysGen != targetGen)
          pendingLSysGen = targetGen;

        if(pendingLSysGen != currentLSysGen && currentGeom.wrapped)
        {
          lsystemGenerate(pendingLSysGen);
          currentLSysGen = pendingLSysGen;
          turtleReset(true);
        }
        currentGeom = turtleStep(kPitchAmt);
      }
    }

    tapModB += TAP_GEOM_SLEW * (tapModTargetB - tapModB);
    tapModC += TAP_GEOM_SLEW * (tapModTargetC - tapModC);

    tapASmooth += tapTimeGlideK * ((float)tapA - tapASmooth);
    float tapReadA = delayReadInterp((float)delayWritePos - tapASmooth);

    for(int t = 0; t < TAP_COUNT; t++)
    {
      repeatWeight[t] += REPEAT_WEIGHT_SLEW_RATE * (repeatTarget[t] - repeatWeight[t]);
      fbWeight[t]     += FB_WEIGHT_SLEW_RATE     * (fbTarget[t]     - fbWeight[t]);
    }

    float tapGrainOut[TAP_COUNT];
    for(int t = 0; t < TAP_COUNT; t++)
      tapGrainOut[t] = grainsProcess(tapGrains[t], GRAINS_PER_TAP);
    float deepGrainOut = grainsProcess(deepGrains, DEEP_GRAIN_COUNT);

    if(sliceCounter == 0)
    {
      inputShaper.update(currentGeom.geomCode, currentGeom.radius2, currentGeom.turnness,
                         currentGeom.bboxAspect, currentGeom.selfNear, currentGeom.cellDensity,
                         currentGeom.spreadN, currentGeom.quadXBias, currentGeom.quadYBias,
                         currentGeom.centroidXBias, currentGeom.centroidYBias, currentGeom.frontierN,
                         kGovern, kDriveCtl, sampleRate);

      resonantBody.update(currentGeom.geomCode, currentGeom.radius2, currentGeom.turnness,
                          currentGeom.bboxAspect, currentGeom.selfNear, currentGeom.cellDensity,
                          currentGeom.spreadN, currentGeom.quadXBias, currentGeom.quadYBias,
                          currentGeom.centroidXBias, currentGeom.centroidYBias, currentGeom.frontierN,
                          kGovern, swBody);

      float geomModMax = TAP_GEOM_MOD_MS * 0.001f * sampleRate;
      tapModTargetB =  geomModMax * clampf(currentGeom.centroidXBias, -1.f, 1.f);
      tapModTargetC = -geomModMax * clampf(currentGeom.centroidYBias, -1.f, 1.f);

      float anchorA = (float)(delayWritePos - tapA);
      float anchorB = (float)(delayWritePos - tapB) - tapModB;
      float anchorC = (float)(delayWritePos - tapC) - tapModC;

      int lenNormal  = (int)clampf((float)slicePeriodSamps * GRAIN_LEN_NORMAL_MUL,  GRAIN_LEN_NORMAL_MIN,  GRAIN_LEN_NORMAL_MAX);
      int lenFifth   = (int)clampf((float)slicePeriodSamps * GRAIN_LEN_FIFTH_MUL,   GRAIN_LEN_FIFTH_MIN,   GRAIN_LEN_FIFTH_MAX);
      int lenOctave  = (int)clampf((float)slicePeriodSamps * GRAIN_LEN_OCTAVE_MUL,  GRAIN_LEN_OCTAVE_MIN,  GRAIN_LEN_OCTAVE_MAX);

      float steer       = clampf(kGovern, 0.f, 1.f);
      float pitchBlend  = clampf(kPitchAmt, 0.f, 1.f);
      float gTurn       = clampf(currentGeom.turnness, 0.f, 1.f);
      float gRadius     = clampf(sqrtf((float)currentGeom.radius2) * 0.10f, 0.f, 1.f);
      float gNear       = clampf(currentGeom.selfNear, 0.f, 1.f);
      float gAspect     = clampf(fabsf(logf(clampf(currentGeom.bboxAspect, 0.25f, 4.f)) / 0.69314718f) * 0.60f, 0.f, 1.f);
      float axisSkew    = clampf((float)(abs(currentGeom.x) - abs(currentGeom.y)) / (float)(abs(currentGeom.x) + abs(currentGeom.y) + 1), -1.f, 1.f);
      float bboxDiag    = sqrtf((float)(currentGeom.bboxW * currentGeom.bboxW + currentGeom.bboxH * currentGeom.bboxH));
      float centerN     = 1.f - clampf(sqrtf((float)currentGeom.radius2) / (bboxDiag + 1.f), 0.f, 1.f);
      float edgeN       = 1.f - centerN;
      float cellN       = clampf(currentGeom.cellDensity, 0.f, 1.f);
      float spreadN     = clampf(currentGeom.spreadN, 0.f, 1.f);
      float quadXBias   = clampf(currentGeom.quadXBias, -1.f, 1.f);
      float quadYBias   = clampf(currentGeom.quadYBias, -1.f, 1.f);
      float centroidX   = clampf(currentGeom.centroidXBias, -1.f, 1.f);
      float centroidY   = clampf(currentGeom.centroidYBias, -1.f, 1.f);
      float frontierN   = clampf(currentGeom.frontierN, 0.f, 1.f);

      bool eventTurn    = (!geoHoldActive) && (currentGeom.symbol == '+' || currentGeom.symbol == '-');
      bool eventWrapped = (!geoHoldActive) && currentGeom.wrapped;

      float laneModA = clampf(
          LANE_A_CENTER_WEIGHT   * centerN   + LANE_A_SPREAD_WEIGHT   * spreadN
        + LANE_A_FRONTIER_WEIGHT * frontierN + LANE_A_CELL_WEIGHT     * cellN
        + LANE_A_AXIS_WEIGHT     * fabsf(axisSkew) + LANE_A_CENTROID_WEIGHT * fabsf(centroidX),
        LANE_A_CLAMP_LO, LANE_A_CLAMP_HI);

      float laneModB = clampf(
          LANE_B_RADIUS_WEIGHT   * gRadius   + LANE_B_NEAR_WEIGHT     * gNear
        + LANE_B_CELL_WEIGHT     * cellN     + LANE_B_SPREAD_WEIGHT   * spreadN
        + LANE_B_AXIS_WEIGHT     * clampf(axisSkew, 0.f, 1.f)
        + LANE_B_CENTROID_WEIGHT * clampf(centroidX, 0.f, 1.f),
        LANE_B_CLAMP_LO, LANE_B_CLAMP_HI);

      float laneModC = clampf(
          LANE_C_TURN_WEIGHT     * gTurn     + LANE_C_NEAR_WEIGHT     * gNear
        + LANE_C_ASPECT_WEIGHT   * gAspect   + LANE_C_EDGE_WEIGHT     * edgeN
        + LANE_C_FRONTIER_WEIGHT * frontierN + LANE_C_CELL_WEIGHT     * cellN
        + LANE_C_AXIS_WEIGHT     * clampf(-axisSkew, 0.f, 1.f)
        + LANE_C_CENTROID_WEIGHT * clampf(-centroidY, 0.f, 1.f),
        LANE_C_CLAMP_LO, LANE_C_CLAMP_HI);

      int head        = currentGeom.heading & 3;
      bool parityFlip = ((abs(currentGeom.x) ^ abs(currentGeom.y)) & 1) != 0;

      repeatTarget[0] = lerpf(0.92f, REPEAT_BASE_WEIGHTS[head][0], steer) * (1.f + laneModA);
      repeatTarget[1] = lerpf(0.38f, REPEAT_BASE_WEIGHTS[head][1], steer) * (1.f + laneModB + (parityFlip ? 0.04f : 0.f));
      repeatTarget[2] = lerpf(0.18f, REPEAT_BASE_WEIGHTS[head][2], steer) * (1.f + laneModC);

      fbTarget[0] = lerpf(1.00f, 1.10f, steer) * (1.f + 0.55f * laneModA);
      fbTarget[1] = lerpf(0.24f, 0.34f, steer) * (1.f + 0.45f * laneModB);
      fbTarget[2] = lerpf(0.08f, 0.12f, steer) * (1.f + 0.45f * laneModC);

      float ampFifth = lerpf(0.15f, 0.30f, steer)
        * (1.f + 0.10f * gRadius + 0.08f * gNear + 0.06f * gAspect + 0.10f * cellN
               + 0.14f * clampf(axisSkew,  0.f, 1.f) + 0.06f * clampf(quadXBias, 0.f, 1.f)
               + 0.08f * clampf(centroidX, 0.f, 1.f));

      float ampOctave = lerpf(0.10f, 0.23f, steer)
        * (1.f + 0.16f * gTurn + 0.12f * gNear + 0.08f * gAspect + 0.10f * cellN
               + 0.14f * clampf(-axisSkew, 0.f, 1.f) + 0.08f * edgeN
               + 0.06f * clampf(-quadYBias, 0.f, 1.f) + 0.12f * frontierN
               + 0.08f * clampf(-centroidY, 0.f, 1.f));
      if(eventTurn) ampOctave *= 1.16f;

      grainSpawnOnTap(1, anchorB, PITCH_RATIO_FIFTH,  lenFifth,  ampFifth);
      grainSpawnOnTap(2, anchorC, PITCH_RATIO_OCTAVE, lenOctave, ampOctave);

      if(swBloom && eventTurn)
      {
        float bloomAmp = lerpf(0.08f, 0.24f, steer)
          + 0.08f * pitchBlend + 0.06f * gTurn + 0.12f * gNear + 0.10f * cellN
          + 0.06f * edgeN + 0.04f * fabsf(axisSkew) + 0.03f * spreadN
          + 0.10f * frontierN + 0.05f * fabsf(centroidX);
        int lenBloomB = (int)clampf((float)slicePeriodSamps * GRAIN_LEN_BLOOM_B_MUL, GRAIN_LEN_BLOOM_MIN, GRAIN_LEN_BLOOM_MAX);
        int lenBloomC = (int)clampf((float)slicePeriodSamps * GRAIN_LEN_BLOOM_C_MUL, GRAIN_LEN_BLOOM_MIN, GRAIN_LEN_BLOOM_MAX);
        grainSpawnOnTap(1, anchorB, PITCH_RATIO_FIFTH,  lenBloomB, bloomAmp * 0.75f);
        grainSpawnOnTap(2, anchorC, PITCH_RATIO_OCTAVE, lenBloomC, bloomAmp);
      }

      if(swRetrace && eventTurn)
      {
        float retraceAmp = lerpf(0.06f, 0.18f, steer)
          + 0.04f * pitchBlend + 0.04f * gTurn + 0.16f * gNear + 0.12f * cellN
          + 0.06f * centerN + 0.03f * spreadN + 0.10f * frontierN + 0.05f * fabsf(centroidY);
        int lenRtrA = (int)clampf((float)slicePeriodSamps * GRAIN_LEN_RETRACE_A_MUL, GRAIN_LEN_RETRACE_MIN, GRAIN_LEN_RETRACE_MAX);
        int lenRtrB = (int)clampf((float)slicePeriodSamps * GRAIN_LEN_RETRACE_B_MUL, GRAIN_LEN_RETRACE_MIN, GRAIN_LEN_RETRACE_MAX);
        int lenRtrC = (int)clampf((float)slicePeriodSamps * GRAIN_LEN_RETRACE_C_MUL, GRAIN_LEN_RETRACE_MIN, GRAIN_LEN_RETRACE_MAX);
        grainSpawnOnTap(0, anchorA, -PITCH_RATIO_UNISON,  lenRtrA, retraceAmp * 0.34f);
        grainSpawnOnTap(1, anchorB, -PITCH_RATIO_FIFTH,   lenRtrB, retraceAmp * 0.56f);
        grainSpawnOnTap(2, anchorC, -PITCH_RATIO_OCTAVE,  lenRtrC, retraceAmp * 0.72f);
      }

      if(currentGeom.spawn && !geoHoldActive)
      {
        float anchor = (currentGeom.tap == 0) ? anchorA
                     : (currentGeom.tap == 1) ? anchorB : anchorC;
        float ratio  = lerpf(PITCH_RATIO_UNISON, currentGeom.pitch, pitchBlend);
        float amp    = (currentGeom.tap == 2) ? lerpf(0.10f, 0.26f, steer)
                                              : lerpf(0.06f, 0.18f, steer);
        grainSpawnOnTap((int)currentGeom.tap, anchor, ratio, lenNormal, amp);
      }

      if(eventWrapped)
      {
        float accentAmp = lerpf(0.10f, 0.24f, clampf(kGovern, 0.f, 1.f))
          + 0.04f * gNear + 0.08f * cellN + 0.03f * centerN
          + 0.03f * gAspect + 0.03f * spreadN + 0.06f * frontierN;
        grainSpawnOnTap(0, anchorA, PITCH_RATIO_UNISON,  lenNormal,  accentAmp);
        grainSpawnOnTap(1, anchorB, PITCH_RATIO_FIFTH,   lenFifth,   accentAmp * 0.85f);
        grainSpawnOnTap(2, anchorC, PITCH_RATIO_OCTAVE,  lenOctave,  accentAmp * 0.85f);
      }

      if(swRetrace && eventWrapped)
      {
        float retraceWrap = lerpf(0.08f, 0.20f, clampf(kGovern, 0.f, 1.f))
          + 0.04f * gRadius + 0.10f * gNear + 0.10f * cellN + 0.04f * gAspect
          + 0.05f * centerN + 0.03f * spreadN + 0.10f * frontierN + 0.05f * fabsf(centroidY);
        int lenWrapA = (int)clampf((float)slicePeriodSamps * GRAIN_LEN_WRAP_A_MUL, GRAIN_LEN_WRAP_MIN, GRAIN_LEN_WRAP_MAX);
        int lenWrapB = (int)clampf((float)slicePeriodSamps * GRAIN_LEN_WRAP_B_MUL, GRAIN_LEN_WRAP_MIN, GRAIN_LEN_WRAP_MAX);
        int lenWrapC = (int)clampf((float)slicePeriodSamps * GRAIN_LEN_WRAP_C_MUL, GRAIN_LEN_WRAP_MIN, GRAIN_LEN_WRAP_MAX);
        grainSpawnOnTap(0, anchorA, -PITCH_RATIO_UNISON,  lenWrapA, retraceWrap * 0.42f);
        grainSpawnOnTap(1, anchorB, -PITCH_RATIO_FIFTH,   lenWrapB, retraceWrap * 0.58f);
        grainSpawnOnTap(2, anchorC, -PITCH_RATIO_OCTAVE,  lenWrapC, retraceWrap * 0.68f);
      }

      if(eventTurn)
      {
        int   tapD    = (int)clampf((float)tapA * TAP_D_DELAY_RATIO, (float)(DELAY_GUARD_SAMPLES + 1024), (float)(DELAY_BUFFER_LEN - 1));
        float anchorD = (float)(delayWritePos - tapD);
        int   lenDeep = (int)clampf((float)slicePeriodSamps * GRAIN_LEN_DEEP_MUL, GRAIN_LEN_DEEP_MIN, GRAIN_LEN_DEEP_MAX);
        float ampDeep = lerpf(0.08f, 0.22f, steer)
          * (1.f + 0.10f * gTurn + 0.06f * gNear + 0.06f * cellN + 0.08f * frontierN);
        grainSpawnDeep(anchorD, 0.5f, lenDeep, ampDeep);
      }

      if(eventWrapped)
      {
        cycleMemDecay(CYCLE_MEM_WRAP_DECAY);
        turtleReset(false);
      }
    }

    float directWet  = tapReadA * repeatWeight[0];
    float octaveWet  = tapGrainOut[1] * repeatWeight[1] + tapGrainOut[2] * repeatWeight[2];
    float octaveSide = tapGrainOut[1] * repeatWeight[1] - tapGrainOut[2] * repeatWeight[2];
    float garnishWet = tapGrainOut[0] * repeatWeight[0] * MIX_GRAIN_A_GARNISH
                     + deepGrainOut   * (MIX_DEEP_OUTPUT_SCALE + MIX_DEEP_GOVERN_SCALE * kGovern);

    float resonIn  = octaveWet  * BODY_RESONIN_OCTAVE_SCALE
                   + garnishWet * BODY_RESONIN_GARNISH_SCALE
                   + tapReadA   * BODY_RESONIN_TAPREAD_SCALE;
    float resonWet = swBody ? softclipf(resonantBody.processWet(resonIn)) : 0.f;

    float fbBus = tapReadA      * fbWeight[0]
                + tapGrainOut[1] * fbWeight[1]
                + tapGrainOut[2] * fbWeight[2]
                + tapGrainOut[0] * fbWeight[0] * MIX_GRAIN_A_FB
                + deepGrainOut  * BODY_FB_INPUT_SCALE;
    if(swBody) fbBus += softclipf(resonantBody.processFeedback(fbBus)) * resonantBody.fbMix;
    fbBus *= clampf(kFeedback, 0.f, FB_MAX_SAFE) * FB_GLOBAL_SCALE;

    fbSafetyLP = fbSafetyLP + FB_LOWPASS_COEF * (fbBus - fbSafetyLP);
    float fbTone = lerpf(fbBus, fbSafetyLP, FB_TONE_MIX * clampf(kFeedback, 0.f, 1.f));

    // Loop limiter: instant attack, slow release, so a held repeat sits at the
    // ceiling instead of either dying or blooming.
    float fbAbs = fabsf(fbTone);
    fbLimEnv += ((fbAbs > fbLimEnv) ? fbLimAtt : fbLimRel) * (fbAbs - fbLimEnv);
    float fbLimTarget = (fbLimEnv > FB_LIMIT_CEILING) ? FB_LIMIT_CEILING / (fbLimEnv + 1e-9f) : 1.f;
    fbLimGain += ((fbLimTarget < fbLimGain) ? fbLimAtt : fbLimRel) * (fbLimTarget - fbLimGain);

    float fbInject = softclipf(fbTone * fbLimGain);

    float rawIn    = inMono * INPUT_PRE_GAIN;
    float shapedIn = inputShaper.process(rawIn);
    float writeIn  = lerpf(rawIn, shapedIn, WRITE_DRY_BLEND_BASE + WRITE_DRY_BLEND_SCALE * clampf(kDriveCtl, 0.f, 1.f));
    delayBuffer[delayWritePos] = softclipf(writeIn + fbInject);
    if(++delayWritePos >= DELAY_BUFFER_LEN) delayWritePos = 0;

    float wet      = (directWet  * MIX_WET_DIRECT_SCALE
                   +  octaveWet  * MIX_WET_OCTAVE_SCALE
                   +  garnishWet * MIX_WET_GARNISH_SCALE
                   +  resonWet   * resonantBody.wetMix) * OUTPUT_POST_GAIN;
    float wetSide = octaveSide * MIX_WET_OCTAVE_SCALE * OUTPUT_POST_GAIN * STEREO_SPREAD;

    // Match the swell's predelay on the wet side so the two stay aligned.
    wetAlignBuf[wetAlignIdx]     = wet;
    wetAlignSideBuf[wetAlignIdx] = wetSide;
    if(++wetAlignIdx >= WET_ALIGN_LEN) wetAlignIdx = 0;
    if(swSwell)
    {
      int wr = wetAlignIdx - fastSwell.delaySamps;
      if(wr < 0) wr += WET_ALIGN_LEN;
      wet     = wetAlignBuf[wr];
      wetSide = wetAlignSideBuf[wr];
    }

    float cleanDry = fastSwell.process(inMono, sampleRate, swSwell);
    float mixN     = clampf(kMix, 0.f, 1.f);
    float dryGain  = 1.f - smoothstepf(MIX_DRY_HOLD, 1.f, mixN);
    float centre   = (cleanDry * dryGain + wet * mixN) * gBoostLin * MASTER_OUTPUT_GAIN;
    float side     = wetSide * mixN * gBoostLin * MASTER_OUTPUT_GAIN;
    float yL       = softclipf(centre + side);
    float yR       = softclipf(centre - side);

    // Crossfade rather than hard-switch the bypass.
    bypassFade += BYPASS_FADE_STEP * ((bypassActive ? 0.f : 1.f) - bypassFade);
    out[0][i] = lerpf(inL, yL, bypassFade);
    out[1][i] = lerpf(inR, yR, bypassFade);
  }
}

void setup()
{
  DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  sampleRate = DAISY.get_samplerate();
  analogReadResolution(POT_BITS);

  pinMode(PIN_SW_BLOOM,    INPUT_PULLUP);
  pinMode(PIN_SW_BODY,     INPUT_PULLUP);
  pinMode(PIN_SW_RETRACE,  INPUT_PULLUP);
  pinMode(PIN_SW_SWELL,    INPUT_PULLUP);
  pinMode(PIN_BTN_BYPASS,  INPUT_PULLUP);
  pinMode(PIN_LED_BYPASS,  OUTPUT);  ledWrite(PIN_LED_BYPASS,  true);   // effect runs at power-up
  pinMode(PIN_BTN_GEOHOLD, INPUT_PULLUP);
  pinMode(PIN_LED_GEOHOLD, OUTPUT);  ledWrite(PIN_LED_GEOHOLD, false);

  for(int i = 0; i < DELAY_BUFFER_LEN; i++) delayBuffer[i] = 0.f;
  delayWritePos = 0;

  grainsResetAll();
  for(int i = 0; i < WET_ALIGN_LEN; i++) { wetAlignBuf[i] = 0.f; wetAlignSideBuf[i] = 0.f; }
  wetAlignIdx = 0;
  inputShaper.reset();
  resonantBody.reset();
  fastSwell.reset(sampleRate);
  fbLimAtt = 1.f - expf(-1.f / (FB_LIMIT_ATT_MS * 0.001f * sampleRate));
  fbLimRel = 1.f - expf(-1.f / (FB_LIMIT_REL_MS * 0.001f * sampleRate));
  fbLimEnv = 0.f; fbLimGain = 1.f;
  tapTimeGlideK = 1.f - expf(-1.f / (TAP_TIME_GLIDE_MS * 0.001f * sampleRate));
  tapASmooth = lerpf(DELAY_TIME_MIN_MS, DELAY_TIME_MAX_MS, kTime) * 0.001f * sampleRate;

  lsystemGenerate(4);
  currentLSysGen = 4;
  pendingLSysGen = 4;
  turtleReset(true);

  currentGeom            = LSystemOut{};
  currentGeom.spawn      = true;
  currentGeom.pitch      = PITCH_RATIO_UNISON;
  currentGeom.symbol     = 'F';
  currentGeom.bboxAspect = 1.f;
  repeatWeight[0] = repeatTarget[0] = 1.00f;
  repeatWeight[1] = repeatTarget[1] = 0.42f;
  repeatWeight[2] = repeatTarget[2] = 0.22f;
  fbWeight[0]     = fbTarget[0]     = 1.00f;
  fbWeight[1]     = fbTarget[1]     = 0.26f;
  fbWeight[2]     = fbTarget[2]     = 0.10f;

  DAISY.begin(AudioCallback);
}

static inline float readKnobNorm(int pin)
{
  return clampf((float)analogRead(pin) / POT_MAX, 0.f, 1.f);
}

void loop()
{
  kTime      += KNOB_SLEW_COEF * (readKnobNorm(A1) - kTime);
  kFeedback  += KNOB_SLEW_COEF * (readKnobNorm(A2) - kFeedback);
  kMix       += KNOB_SLEW_COEF * (readKnobNorm(A3) - kMix);
  kStructure += KNOB_SLEW_COEF * (readKnobNorm(A4) - kStructure);

  // These two must persist between calls, like the other knobs; as locals
  // initialised to zero they could only ever reach KNOB_SLEW_COEF of the pot.
  static float kMacro = 0.f;
  static float kA6    = 0.f;
  kMacro += KNOB_SLEW_COEF * (readKnobNorm(A5) - kMacro);
  kA6    += KNOB_SLEW_COEF * (readKnobNorm(A6) - kA6);

  // Spread across the travel: Govern uses the whole knob, Pitch Amount joins
  // it from the middle, Drive fills the lower half and Boost the upper half.
  kGovern   = smoothstepf(0.00f, 1.00f, kMacro);
  kPitchAmt = smoothstepf(0.45f, 1.00f, kMacro);
  kDriveCtl = smoothstepf(0.00f, 0.50f, kA6);

  float boostDb = OUTPUT_BOOST_MAX_DB * clampf(smoothstepf(0.50f, 1.00f, kA6), 0.f, 1.f);
  gBoostLin     = powf(10.f, boostDb / 20.f);

  swBloom   = (digitalRead(PIN_SW_BLOOM)   == LOW);
  swBody    = (digitalRead(PIN_SW_BODY)    == LOW);
  swRetrace = (digitalRead(PIN_SW_RETRACE) == LOW);
  swSwell   = (digitalRead(PIN_SW_SWELL)   == LOW);

  struct ButtonState
  {
    bool rawLast;
    uint32_t edgeMs;
    bool pressArmed;
    bool consumed;
    uint32_t pressStartMs;
  };

  static ButtonState btnBypass  = {};
  static ButtonState btnGeoHold = {};

  auto updateButton = [](ButtonState& btn, int pin, bool& latched, bool& held, uint32_t now)
  {
    bool pressed = (digitalRead(pin) == LOW);

    if(pressed != btn.rawLast) { btn.rawLast = pressed; btn.edgeMs = now; }

    if((now - btn.edgeMs) > BUTTON_DEBOUNCE_MS)
    {
      if(pressed && !btn.pressArmed)
        { btn.pressArmed = true; btn.consumed = false; btn.pressStartMs = now; }

      if(btn.pressArmed && pressed && !btn.consumed && (now - btn.pressStartMs) >= BUTTON_HOLD_TIME_MS)
        { held = true; btn.consumed = true; }

      if(!pressed && btn.pressArmed)
      {
        if(btn.consumed) held    = false;
        else             latched = !latched;
        btn.pressArmed = false;
        btn.consumed   = false;
      }
    }
  };

  uint32_t now = millis();
  updateButton(btnBypass,  PIN_BTN_BYPASS,  bypassLatched,  bypassHeld,  now);
  updateButton(btnGeoHold, PIN_BTN_GEOHOLD, geoHoldLatched, geoHoldHeld, now);

  bypassActive  = bypassLatched  || bypassHeld;
  geoHoldActive = geoHoldLatched || geoHoldHeld;

  ledWrite(PIN_LED_BYPASS,  !bypassActive);
  ledWrite(PIN_LED_GEOHOLD, geoHoldActive);

  delay(2);
}
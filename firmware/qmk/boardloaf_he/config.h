#pragma once

#define HE_MATRIX_ROWS 3 // Rows per half
#define MATRIX_COLS 6 // Columns per half

#define MATRIX_ROW_PINS \
    { GP26, GP27, GP28 }

#define ROWS_PER_COL \
    { 3, 3, 3, 3, 3, 3 }

#define AMUX_MAX_COLS_COUNT 6

#define AMUX_SEL_PINS \
    { GP6, GP7, GP8 }

#define AMUX_COL_CHANNELS \
    { 0, 1, 2, 3, 4, 5 }

#define DEFAULT_ACTUATION_MODE 0
// "Light" preset based on 480-330 sensor range (~150 counts travel):
// APC actuation at 30% travel, release at 20% travel (10% hysteresis)
// RT deadzone at 30%, act offset ~10 counts (6.7% of travel), rel offset ~8 counts (5.3%)
#define DEFAULT_MODE_0_ACTUATION_LEVEL 300
#define DEFAULT_MODE_0_RELEASE_LEVEL 200
#define DEFAULT_MODE_1_INITIAL_DEADZONE_OFFSET 300
#define DEFAULT_MODE_1_ACTUATION_OFFSET 10
#define DEFAULT_MODE_1_RELEASE_OFFSET 8
#define DEFAULT_BOTTOM_DEADZONE 50
#define DEFAULT_EXTREMUM 1023
#define NOISE_CEILING_THRESHOLD 30
#define BOTTOMING_CALIBRATION_THRESHOLD 50
#define DEFAULT_NOISE_CEILING_SAMPLING_COUNT 30
#define DEFAULT_BOTTOMING_READING 0
#define DEFAULT_CALIBRATION_STARTER true

#define EECONFIG_KB_DATA_SIZE 217

#define VIA_CUSTOM_LIGHTING_ENABLE

#define WEAK_BITRATE

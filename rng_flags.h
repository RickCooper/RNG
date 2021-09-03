
// #define at most one of the following to 1:
#define SWITCH_DIRECTION_BIAS    0
#define REJECT_SMART_V1          0
#define REJECT_SMART_V2          0

// The last two are "smarter" ways to reject putative responses as non-random. We
// might reject because the gap between the current and the previous is the same
// as the gap between the previous and the one before that (REJECT_SMART_V1), or
// because the current represents a continuation of a move in the same direction
// as the previous two responses (REJECT_SMART_V2). The latter (V2) should result
// in higher TPI. 

#define RT_FLAGS "BASE"
#define LOG_FILE "FIT_BASE_CTL.log"

#if SWITCH_DIRECTION_BIAS
#define RT_FLAGS "SD"
#define LOG_FILE "FIT_SD_CTL.log"
#endif

#if REJECT_SMART_V1
#define RT_FLAGS "V1"
#define LOG_FILE "FIT_V1_CTL.log"
#endif

#if REJECT_SMART_V2
#define RT_FLAGS "V2"
#define LOG_FILE "FIT_V2_CT.log"
#endif



#ifndef _COMMON_H
#define _COMMON_H
typedef enum
{
  ERASEING_FILES,
  CAPTURING_IMG,
  PLAYBACK,
  RECORDING,
  RECORD_END,
  STREAMING,
  END
} States;

extern States currentStatus;
#endif //_COMMON_H

/*
  NOTE: This is not intended to be a functional program. Its only purpose
  is to act as a tool to find out what portions of the Octasic API kit we
  actually need to link into our drivers. As such, it references every API
  call that the actual drivers use, and we let the compiler and linker tell
  us what parts of each API module are actually needed to successfully
  build this program.
 */
#include "oct6100api/oct6100_api.h"

int main(int argc, char **argv)
{
	tPOCT6100_INSTANCE_API pApiInstance;
	UINT32 ulResult;
	tOCT6100_CHANNEL_MODIFY modify;
	tOCT6100_INTERRUPT_FLAGS InterruptFlags;
	tOCT6100_TONE_EVENT tonefound;
	tOCT6100_EVENT_GET_TONE tonesearch;
	tOCT6100_CHIP_OPEN ChipOpen;
	tOCT6100_GET_INSTANCE_SIZE InstanceSize;
	tOCT6100_CHANNEL_OPEN ChannelOpen;
	tOCT6100_TONE_DETECTION_ENABLE enable;
	tOCT6100_CHIP_CLOSE ChipClose;
	tOCT6100_API_GET_CAPACITY_PINS CapacityPins;

	Oct6100ChannelModifyDef(&modify);
	ulResult = Oct6100ChannelModify(pApiInstance, &modify);
	Oct6100InterruptServiceRoutineDef(&InterruptFlags);
	Oct6100InterruptServiceRoutine(pApiInstance, &InterruptFlags);
	Oct6100EventGetToneDef(&tonesearch);
	ulResult = Oct6100EventGetTone(pApiInstance, &tonesearch);
	Oct6100ChipOpenDef(&ChipOpen);
	Oct6100GetInstanceSizeDef(&InstanceSize);
	ulResult = Oct6100GetInstanceSize(&ChipOpen, &InstanceSize);
	ulResult = Oct6100ChipOpen(pApiInstance, &ChipOpen);
	Oct6100ChannelOpenDef(&ChannelOpen);
	ulResult = Oct6100ChannelOpen(pApiInstance, &ChannelOpen);
	Oct6100ToneDetectionEnableDef(&enable);
	Oct6100ToneDetectionEnable(pApiInstance, &enable);
	Oct6100ChipCloseDef(&ChipClose);
	ulResult = Oct6100ChipClose(pApiInstance, &ChipClose);
	Oct6100ApiGetCapacityPinsDef(&CapacityPins);
	ulResult = Oct6100ApiGetCapacityPins(&CapacityPins);

	return 0;
}

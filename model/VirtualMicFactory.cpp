#include "VirtualMicFactory.h"

#include "VirtualMic.h"

#ifdef QRTWHISPER_MIC_BACKEND_PACTL
#include "PactlVirtualMic.h"
#else
#include "PulseAudioVirtualMic.h"
#endif

std::unique_ptr<VirtualMic> makeVirtualMic() {
#ifdef QRTWHISPER_MIC_BACKEND_PACTL
    return std::make_unique<PactlVirtualMic>();
#else
    return std::make_unique<PulseAudioVirtualMic>();
#endif
}

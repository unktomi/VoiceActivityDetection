#pragma once
#include "Engine.h"
#include "MediaPlayer.h"
#include "IMediaSink.h"
#include "IMediaAudioTrack.h"
#include "VoiceActivityDetection.h"
#include "VAD.generated.h"

class VADSink : public IMediaSink {
	FWeakObjectPtr Target;
public:
	VADSink() : Target(nullptr) {}
	VADSink(class UVAD *U);
	virtual void ProcessMediaSample(const void* Buffer, uint32 BufferSize, FTimespan Duration, FTimespan Time) override;
};

UCLASS()
class UVAD : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual void ProcessMediaSample(const void* Buffer, uint32 BufferSize, FTimespan Duration, FTimespan Time);
	virtual void FinishDestroy() override;
	virtual void UseMediaPlayer(UMediaPlayer *Player, int32 &Channels, int32 &SampleRate);
	virtual void StopUsingMediaPlayer(UMediaPlayer *Player);
	// Sets the VAD operating mode. A more aggressive (higher mode) VAD is more
	// restrictive in reporting speech. Put in other words the probability of being
	// speech when the VAD returns 1 is increased with increasing mode. As a
	// consequence also the missed detection rate goes up.
	// - Mode   [i]   : Aggressiveness mode (0, 1, 2, or 3).
	virtual void SetMode(int32 Mode);

	/** Gets an event delegate that is invoked when voice is active */
	DECLARE_EVENT(UVAD, FOnVoiceActive)
	FOnVoiceActive& OnActive()
	{
		return ActiveEvent;
	}
	/** Gets an event delegate that is invoked when voice input is captured */
	DECLARE_EVENT_OneParam(UVAD, FOnVoiceInputCaptured, TArray<uint8>)
	FOnVoiceInputCaptured& OnVoiceInputCaptured()
	{
		return VoiceInputCapturedEvent;
	}

	/** Gets an event delegate that is invoked when voice is inactive */
	DECLARE_EVENT(UVAD, FOnVoiceInactive)
	FOnVoiceInactive& OnInactive()
	{
		return InactiveEvent;
	}

	void SetInactivityThreshold(float Seconds)
	{
		InactivityThreshold = Seconds;
	}

	void SetContinuous(bool Continuous)
	{
		bContinuous = Continuous;
	}

	void SetFrameLength(int32 Milliseconds)
	{
		VAD_FRAME_MILLIS = Milliseconds;
	}

private:

	struct WebRtcVadInst *Handle;

	FOnVoiceActive ActiveEvent;
	FOnVoiceInactive InactiveEvent;
	FOnVoiceInputCaptured VoiceInputCapturedEvent;

	TSharedRef<VADSink, ESPMode::ThreadSafe> Sink;
	TSharedPtr<IMediaAudioTrack, ESPMode::ThreadSafe> AudioTrack;
	TArray<int16> SampleBuffer;
	TArray<uint8> CaptureBuffer;
	TArray<int16> ResampleBuffer;
	TArray<int16> OutputBuffer; // Resampler output
	TArray<int16> InputBuffer;
	bool bActive;
	bool bContinuous;
	float InactivityThreshold; // in seconds
	uint32 InactiveCount; // in samples

	struct SpeexResamplerState_ *Resampler;
	uint32 VAD_SAMPLES_PER_SEC; // webrtc_vad sample rate in hz, one of 8000, 16000, 32000, 48000
	uint32 VAD_FRAME_MILLIS; // duration of vad sample frame in milliseconds
	uint32 Channels;
	uint32 SamplesPerSecond;
	FCriticalSection CriticalSection;
	double LastActiveTime;
};

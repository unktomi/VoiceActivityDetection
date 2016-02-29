#include "VoiceActivityDetectionPrivatePCH.h"
#include "VoiceActivityDetection.h"
#include "VAD.h"
#include "IMediaPlayer.h"
#include "webrtc/typedefs.h"
#include "webrtc/common_audio/vad/include/webrtc_vad.h"
//#include "../../Thirdparty/webrtc/common_audio/resampler/include/resampler.h"
#include "speex/speex_resampler.h"
#include <stdio.h>


DEFINE_LOG_CATEGORY(LogVAD);

VADSink::VADSink(UVAD *Detector) :Target(Detector) {}

void VADSink::ProcessMediaSample(const void* Buffer, uint32 BufferSize, FTimespan Duration, FTimespan Time)
{
	UObject *U = Target.Get();
	if (U != nullptr)
	{
		((UVAD*)U)->ProcessMediaSample(Buffer, BufferSize, Duration, Time);
	}
}

UVAD::UVAD(const class FObjectInitializer& PCIP)
	: Super(PCIP),
	Sink(new VADSink(this)),
	bActive(false),
	bContinuous(false),
	InactivityThreshold(1.0f),
	InactiveCount(0),
	Resampler(nullptr),
	VAD_FRAME_MILLIS(20)
{
	int32 result = WebRtcVad_Create(&Handle);
	if (result < 0)
	{
		UE_LOG(LogVAD, Error, TEXT("WebRtcVad_Create Failed"));
	}
	else
	{
		result = WebRtcVad_Init(Handle);
		if (result < 0)
		{
			UE_LOG(LogVAD, Error, TEXT("WebRtcVad_Init Failed"));
		}
	}
}

void UVAD::UseMediaPlayer(UMediaPlayer *Player, int32 &Channels, int32 &SampleRate)
{
	FScopeLock Lock(&CriticalSection);
	bActive = false; // Reset
	if (Player != nullptr)
	{
		if (Player->GetPlayer()->GetAudioTracks().Num() < 1)
		{
			UE_LOG(LogVAD, Error, TEXT("Media Player is invalid"));
		}
		else
		{
			AudioTrack = Player->GetPlayer()->GetAudioTracks()[0];
		}
	}
	if (AudioTrack.IsValid())
	{
		this->Channels = Channels = AudioTrack->GetNumChannels();
		this->SamplesPerSecond = SampleRate = AudioTrack->GetSamplesPerSecond();	
		AudioTrack->GetStream().AddSink(Sink);
	}
	else
	{
		this->Channels = Channels;
		this->SamplesPerSecond = SampleRate;
	}
	VAD_SAMPLES_PER_SEC = SamplesPerSecond > 32000 ? 32000 : SamplesPerSecond > 16000 ? 16000 : 8000; // webrtc_vad only supports certain rates
	if (Resampler == nullptr)
	{
		int32 err;
		Resampler = speex_resampler_init(1,
			SamplesPerSecond,
			VAD_SAMPLES_PER_SEC,
			5,
			&err);

		if (Resampler == nullptr)
		{
			UE_LOG(LogVAD, Error, TEXT("Couldn't allocate resampler"));
			return;
		}
	}
	else
	{
		speex_resampler_set_rate(Resampler, SamplesPerSecond, VAD_SAMPLES_PER_SEC);
	}
}

void UVAD::SetMode(int32 Mode)
{
	int32 rc = WebRtcVad_set_mode(Handle, Mode);
	if (0 != rc)
	{
		UE_LOG(LogVAD, Error, TEXT("WebRtcVad_set_mode Error"));
	}
}

void UVAD::StopUsingMediaPlayer(UMediaPlayer *Player)
{
	FScopeLock Lock(&CriticalSection);
	if (AudioTrack.IsValid())
	{
		AudioTrack->GetStream().RemoveSink(Sink);
		AudioTrack = nullptr;
	}
}

void UVAD::ProcessMediaSample(const void* Buffer, uint32 BufferSize, FTimespan Duration, FTimespan Time)
{
	FScopeLock Lock(&CriticalSection);
	if (BufferSize == 0)
	{
		if (bActive)
		{
			// flush
			double Now = FPlatformTime::Seconds();
			if (Now - LastActiveTime > InactivityThreshold)
			{
				
				if (CaptureBuffer.Num() > 0)
				{
					TArray<uint8> Copy = CaptureBuffer;
					CaptureBuffer = TArray<uint8>();
					OnVoiceInputCaptured().Broadcast(Copy);
				}
				bActive = false;
				OnInactive().Broadcast();

			}
		}
		return;
	}
	LastActiveTime = FPlatformTime::Seconds();
	uint32 SamplesAvailableInBuffer = BufferSize / sizeof(int16) / Channels;
	uint32 SamplesAvailable = InputBuffer.Num() / Channels + SamplesAvailableInBuffer;
	const uint32 RequiredSamples = SamplesPerSecond / (1000 / VAD_FRAME_MILLIS);
	if (SamplesAvailable < RequiredSamples)
	{
		InputBuffer.Append((int16*)Buffer, BufferSize / (sizeof(int16)));
		return;
	}
	//webrtc::Resampler Resampler;
	ResampleBuffer.Reset(SamplesAvailable);
	for (uint32 i = 0; i < InputBuffer.Num() / Channels; i++)
	{
		double Mono = 0;
		for (uint32 j = 0; j < Channels; j++)
		{
			Mono += InputBuffer[i * Channels + j];
		}
		ResampleBuffer.Add(int16(Mono / Channels));
	}
	for (uint32 i = 0; i < SamplesAvailableInBuffer; i++)
	{
		double Mono = 0;
		for (uint32 j = 0; j < Channels; j++)
		{
			int16 Sample = ((int16*)Buffer)[i * Channels + j];
			Mono += Sample;
		}

		ResampleBuffer.Add(int16(Mono / Channels));
	}
	uint32 OutLength;
	uint32 InLength = ResampleBuffer.Num();
	OutputBuffer.Init(0, OutLength = ResampleBuffer.Num());
	int result = speex_resampler_process_int(Resampler, 0, ResampleBuffer.GetData(), &InLength, OutputBuffer.GetData(), &OutLength);
	if (InLength != ResampleBuffer.Num())
	{
		UE_LOG(LogVAD, Error, TEXT("Error in speex_resampler_process_int"));
		return;
	}
	else
	{
		if (false) UE_LOG(LogVAD, Display, TEXT("Resampled %d %dhz Samples to %d %dhz Samples"), ResampleBuffer.Num(), SamplesPerSecond, OutLength, VAD_SAMPLES_PER_SEC);
	}
	const uint32 BufSize = VAD_SAMPLES_PER_SEC / (1000 / VAD_FRAME_MILLIS); // webrtc_vad only supports certain frame sizes, so batch by 10ms
	bool ThisBufferActive = false;
	SamplesAvailable = (uint32)OutLength;

	for (uint32 i = 0; i < SamplesAvailable && !ThisBufferActive; i += BufSize) {
		SampleBuffer.Reset();
		uint32 SampleCount = 0;
		if (WebRtcVad_ValidRateAndFrameLength(VAD_SAMPLES_PER_SEC, BufSize) < 0)
		{
			UE_LOG(LogVAD, Error, TEXT("Invalid Audio Frame Channels: %d SamplesPerSecond: %d, Length: %u"), Channels, VAD_SAMPLES_PER_SEC, SamplesAvailable);
		}
		for (uint32 j = 0; j < BufSize; j++)
		{
			uint32 k = i + j;
			if (k < SamplesAvailable)
			{
				SampleBuffer.Add(OutputBuffer[k]);
				SampleCount++;
			}
			else
			{
				SampleBuffer.Add(int16(0));
			}
		}

		//int WebRtcVad_Process(VadInst* handle, int fs, const int16_t* audio_frame,
		//	int frame_length);
		int32 rc = WebRtcVad_Process(Handle, VAD_SAMPLES_PER_SEC, SampleBuffer.GetData(), BufSize);
		switch (rc)
		{
		case 1:
		{
			ThisBufferActive = true;
			break;
		}
		case 0:
		{
			break;
		}

		default:
		{
			UE_LOG(LogVAD, Error, TEXT("WebRtcVad_Process Error"));
			break;
		}
		}
	}
	if (ThisBufferActive)
	{
		if (false) UE_LOG(LogVAD, Display, TEXT("ThisBuffer is Active"));
		InactiveCount = 0;
		if (!bActive)
		{
			OnActive().Broadcast();
			UE_LOG(LogVAD, Display, TEXT("Voice is Active"));
			bActive = true;
		}
		if (!bContinuous)
		{
			if (InputBuffer.Num() > 0)
			{
				CaptureBuffer.Append((uint8*)InputBuffer.GetData(), InputBuffer.Num() * sizeof(int16));
			}
			CaptureBuffer.Append((uint8*)Buffer, BufferSize);
		}
		else
		{
			TArray<uint8> Samples;
			if (InputBuffer.Num() > 0)
			{
				Samples.Append((uint8*)InputBuffer.GetData(), InputBuffer.Num() * sizeof(int16));
			}
			Samples.Append((const uint8*)Buffer, (int32)BufferSize);
			OnVoiceInputCaptured().Broadcast(Samples);
		}
	}
	else
	{
		InactiveCount += SamplesAvailableInBuffer;
		if (bActive)
		{
			if (InactiveCount > SamplesPerSecond * InactivityThreshold)
			{
				TArray<uint8> Copy = CaptureBuffer;
				CaptureBuffer = TArray<uint8>();
				OnVoiceInputCaptured().Broadcast(Copy);
				OnInactive().Broadcast();
				bActive = false;
				UE_LOG(LogVAD, Display, TEXT("Voice is Inactive"));
			}
			else if (!bContinuous)
			{
				if (InputBuffer.Num() > 0)
				{
					CaptureBuffer.Append((uint8*)InputBuffer.GetData(), InputBuffer.Num() * sizeof(int16));
				}
				CaptureBuffer.Append((uint8*)Buffer, BufferSize);
			}
			else
			{
				TArray<uint8> Samples;
				if (InputBuffer.Num() > 0)
				{
					Samples.Append((uint8*)InputBuffer.GetData(), InputBuffer.Num() * sizeof(int16));
				}
				Samples.Append((const uint8*)Buffer, (int32)BufferSize);
				OnVoiceInputCaptured().Broadcast(Samples);
			}
		}
	}
	InputBuffer.Reset(0);
	//UE_LOG(LogVAD, Display, TEXT("Inactive Count %u"), InactiveCount);
}

void UVAD::FinishDestroy()
{
	if (Resampler != nullptr)
	{
		speex_resampler_destroy(Resampler);
		Resampler = nullptr;
	}
	if (Handle != nullptr)
	{
		WebRtcVad_Free(Handle);
		Handle = nullptr;
	}
	Super::FinishDestroy();
}

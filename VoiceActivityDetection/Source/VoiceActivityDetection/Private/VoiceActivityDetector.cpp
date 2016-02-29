#include "VoiceActivityDetectionPrivatePCH.h"
#include "VoiceActivityDetector.h"
#include "IMediaPlayer.h"
#include "IMediaAudioTrack.h"
#include "VoiceModule.h"
#include "VoiceCapture.h"



AVoiceActivityDetector::AVoiceActivityDetector(const class FObjectInitializer& PCIP)
	: Super(PCIP),
	Detector(nullptr),
	InputSource(nullptr),
	Channels(0),
	SamplesPerSecond(0),
	InactivityThresholdInSeconds(1.0f),
	VAD_FrameDurationInMilliseconds(20)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AVoiceActivityDetector::HandleActiveVoice()
{
	Commands.Enqueue(FSimpleDelegate::CreateUObject(this, &AVoiceActivityDetector::OnSpeechInputStarted));
}

void AVoiceActivityDetector::HandleVoiceCapture(TArray<uint8> Samples)
{
	Commands.Enqueue(FSimpleDelegate::CreateUObject(this, &AVoiceActivityDetector::BroadcastSpeechInputCaptured, Samples));
}

void AVoiceActivityDetector::HandleInactiveVoice()
{
	Commands.Enqueue(FSimpleDelegate::CreateUObject(this, &AVoiceActivityDetector::OnSpeechInputStopped));
}

void AVoiceActivityDetector::BroadcastSpeechInputCaptured(TArray<uint8> Samples)
{
	OnSpeechInputCaptured(Samples, Channels, SamplesPerSecond);
}

// Called when the game starts or when spawned,
void AVoiceActivityDetector::BeginPlay()
{
	if (Detector == nullptr)
	{
		Detector = NewObject<UVAD>();
		Detector->OnActive().AddUObject(this, &AVoiceActivityDetector::HandleActiveVoice);
		Detector->OnInactive().AddUObject(this, &AVoiceActivityDetector::HandleInactiveVoice);
		Detector->OnVoiceInputCaptured().AddUObject(this, &AVoiceActivityDetector::HandleVoiceCapture);
	}
	Detector->SetMode(Mode);
	Detector->SetInactivityThreshold(InactivityThresholdInSeconds);
	Detector->SetFrameLength(VAD_FrameDurationInMilliseconds); 
	Detector->SetContinuous(bContinuous);
	if (InputSource != nullptr)
	{
		int32 c;
		int32 r;
		Detector->UseMediaPlayer(InputSource, c, r);
		Channels = c;
		SamplesPerSecond = r;
	}
	else
	{
		if (!VoiceCapture.IsValid())
		{
			VoiceCapture = FVoiceModule::Get().CreateVoiceCapture();
		}
		VoiceCapture->Start();
		Channels = 1;
		SamplesPerSecond = 16000;
		Detector->UseMediaPlayer(nullptr, Channels, SamplesPerSecond);
	}
	Super::BeginPlay();
}

// Called every frame
void AVoiceActivityDetector::Tick(float DeltaSeconds)
{
	// process pending commands
	CommandDelegate Command;

	while (Commands.Dequeue(Command))
	{
		Command.Execute();
	}
	if (VoiceCapture.IsValid())
	{
		uint32 BytesAvailable = 0;
		EVoiceCaptureState::Type CaptureState = VoiceCapture->GetCaptureState(BytesAvailable);
		if (CaptureState == EVoiceCaptureState::Ok && BytesAvailable > 0)
		{
			CaptureBuffer.SetNumUninitialized(BytesAvailable);
			uint32 ReadBytes;
			VoiceCapture->GetVoiceData(CaptureBuffer.GetData(), BytesAvailable, ReadBytes);	
			Detector->ProcessMediaSample(CaptureBuffer.GetData(), ReadBytes, 0, 0);
		}
		else
		{
			Detector->ProcessMediaSample(nullptr, 0, 0, 0);
		}
	}
	Super::Tick(DeltaSeconds);

}

// Called when the game ends
void AVoiceActivityDetector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Detector->StopUsingMediaPlayer(InputSource);
	Super::EndPlay(EndPlayReason);
	if (VoiceCapture.IsValid())
	{
		VoiceCapture->Stop();
	}
}
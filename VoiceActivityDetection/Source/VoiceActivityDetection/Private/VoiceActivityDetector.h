#pragma once
#include "Engine.h"
#include "Array.h"
#include "MediaPlayer.h"
#include "VAD.h"
#include "VoiceActivityDetector.generated.h"



UCLASS(BlueprintType, Blueprintable)
class AVoiceActivityDetector : public AActor
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintImplementableEvent)
		void OnSpeechInputStarted();

	UFUNCTION(BlueprintImplementableEvent)
		void OnSpeechInputStopped();

	UFUNCTION(BlueprintImplementableEvent)
		void OnSpeechInputCaptured(const TArray<uint8> &Samples, int32 Channels, int32 SamplesPerSecond);

	UPROPERTY(Category = "Voice Activity Detection", EditAnywhere, BlueprintReadWrite)
		UMediaPlayer *InputSource;
	UPROPERTY(Category = "Voice Activity Detection", EditAnywhere, BlueprintReadWrite)
		int32 Mode;
	UPROPERTY(Category = "Voice Activity Detection", BlueprintReadOnly)
		int32 Channels;
	UPROPERTY(Category = "Voice Activity Detection", BlueprintReadOnly)
		int32 SamplesPerSecond;
	UPROPERTY(Category = "Voice Activity Detection", EditAnywhere, BlueprintReadWrite)
		float InactivityThresholdInSeconds;
	UPROPERTY(Category = "Voice Activity Detection", EditAnywhere, BlueprintReadWrite)
		bool bContinuous;
	UPROPERTY(Category = "Voice Activity Detection", BlueprintReadonly)
		int32 VAD_FrameDurationInMilliseconds;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaSeconds) override;

	// Called when the game ends
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY()
		UVAD *Detector;

private:

	/** Events have to occur on the main thread, so we have this queue to feed the ticker */
	DECLARE_DELEGATE(CommandDelegate)
	/** Holds the router command queue. */
	TQueue<CommandDelegate, EQueueMode::Mpsc> Commands;

	void HandleActiveVoice();
	void HandleInactiveVoice();
	void HandleVoiceCapture(TArray<uint8> Samples);

	void BroadcastSpeechInputCaptured(TArray<uint8> Samples);
	TSharedPtr<class IVoiceCapture> VoiceCapture;
	TArray<uint8> CaptureBuffer;

};

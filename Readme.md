Contains a UE-4 Blueprint Actor <code>VoiceActivityDetector</code> which provides the following events:
  
	void OnSpeechInputStarted()

	void OnSpeechInputStopped()

	void OnSpeechInputCaptured(const TArray<uint8> &Samples, int32 Channels, int32 SamplesPerSecond)
	
Input comes from a <code>MediaPlayer</code> so you'll need the fixes from here: https://github.com/unktomi/UnrealEngine/tree/4.10-with-media-fixes. 
Note that those fixes are windows-only at this point. With those fixes you can access the microphone by opening a media player with the <code>mic://</code> url. 

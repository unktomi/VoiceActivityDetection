// Some copyright should be here...

#pragma once

#include "ModuleManager.h"
#include "Engine.h"

VOICEACTIVITYDETECTION_API DECLARE_LOG_CATEGORY_EXTERN(LogVAD, Display, All);


class FVoiceActivityDetectionModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
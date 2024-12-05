// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/MediaAssets/Public/MediaSoundComponent.h"
#include "Runtime/Engine/Classes/Sound/SoundGenerator.h"
#include "Runtime/SignalProcessing/Public/DSP/SpectrumAnalyzer.h"
#include "Runtime/MediaUtils/Public/MediaSampleQueue.h"
#include "MediaSoundPitchShiftComponent.generated.h"

class FMediaSoundPitchShiftComponentClockSink;

// Class implements an ISoundGenerator to feed decoded audio to audio renderering async tasks
class FMediaSoundPitchShiftGenerator : public ISoundGenerator
{
public:
	struct FSoundGeneratorParams
	{
		int32 SampleRate = 0;
		int32 NumChannels = 0;

		TSharedPtr<FMediaAudioSampleQueue, ESPMode::ThreadSafe> SampleQueue;

		bool bSpectralAnalysisEnabled = false;
		bool bEnvelopeFollowingEnabled = false;

		int32 EnvelopeFollowerAttackTime = 0;
		int32 EnvelopeFollowerReleaseTime = 0;

		Audio::FSpectrumAnalyzerSettings SpectrumAnalyzerSettings;
		TArray<float> FrequenciesToAnalyze;

		float CachedRate = 0.0f;
		FTimespan CachedTime;
		FTimespan LastPlaySampleTime;
	};

	FMediaSoundPitchShiftGenerator(FSoundGeneratorParams& InParams);

	virtual ~FMediaSoundPitchShiftGenerator();

	virtual void OnEndGenerate() override;

	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;
	virtual int32 OnGenerateAudio_Origin(float* OutAudio, int32 NumSamples);

	void SetCachedData(float InCachedRate, const FTimespan& InCachedTime);
	void SetLastPlaySampleTime(const FTimespan& InLastPlaySampleTime);

	void SetEnableSpectralAnalysis(bool bInSpectralAnlaysisEnabled);
	void SetEnableEnvelopeFollowing(bool bInEnvelopeFollowingEnabled);

	void SetSpectrumAnalyzerSettings(Audio::FSpectrumAnalyzerSettings::EFFTSize InFFTSize, const TArray<float>& InFrequenciesToAnalyze);
	void SetEnvelopeFollowingSettings(int32 InAttackTimeMsec, int32 InReleaseTimeMsec);

	void SetSampleQueue(TSharedPtr<FMediaAudioSampleQueue, ESPMode::ThreadSafe>& InSampleQueue);

	TArray<FMediaSoundComponentSpectralData> GetSpectralData() const;
	TArray<FMediaSoundComponentSpectralData> GetNormalizedSpectralData() const;
	float GetCurrentEnvelopeValue() const { return CurrentEnvelopeValue; }

	FTimespan GetLastPlayTime() const { return LastPlaySampleTime.Load(); }

private:

	FSoundGeneratorParams Params;

	/** The audio resampler. */
	FMediaAudioResampler Resampler;

	/** Scratch buffer to mix in source audio to from decoder */
	Audio::AlignedFloatBuffer AudioScratchBuffer;

	/** Spectrum analyzer used for analyzing audio in media. */
	mutable Audio::FAsyncSpectrumAnalyzer SpectrumAnalyzer;

	Audio::FEnvelopeFollower EnvelopeFollower;

	TAtomic<float> CachedRate;
	TAtomic<FTimespan> CachedTime;
	TAtomic<FTimespan> LastPlaySampleTime;

	float CurrentEnvelopeValue = 0.0f;
	bool bEnvelopeFollowerSettingsChanged = false;

	mutable FCriticalSection AnalysisCritSect;
	mutable FCriticalSection SampleQueueCritSect;

//-------------------- 自定义内容 ---------------------------
public:
	double smbAtan2(double x, double y);
	void smbFft(float* fftBuffer, long fftFrameSize, long sign);
	void smbPitchShift(float pitchShift, long numSampsToProcess, long fftFrameSize, long osamp, float sampleRate, float* indata, float* outdata);

	void UpdatePitchShift(float InNewPitchSemitones);
	
	int32 SampleRate;
	float pitchSemitones = 0.0f;
	float pitchShiftFrameRatio = 1.0f;
	float* inputDataPtr;
	int32 inputBufferSize = 0;
//-------------------- 自定义内容 ---------------------------
};

/**
 * Implements a sound component for playing a media player's audio output.
 */
//class WEWORLD_API UMediaSoundPitchShiftComponent
UCLASS(ClassGroup = Media, editinlinenew, meta = (BlueprintSpawnableComponent))
class WEWORLD_API UMediaSoundPitchShiftComponent : public USynthComponent
{
	GENERATED_BODY()

//-------------------- 自定义内容 ---------------------------
public:

	UFUNCTION(BlueprintCallable, Category = "Media|MediaSoundPitchShiftComponent")
		void UpdatePitchShift(float InNewPitchSemitones);
//-------------------- 自定义内容 ---------------------------

public:
	/** Media sound channel type. */
	UPROPERTY(EditAnywhere, Category = "Media")
		EMediaSoundChannels Channels;

	/** Dynamically adjust the sample rate if audio and media clock desynchronize. */
	UPROPERTY(EditAnywhere, Category = "Media", AdvancedDisplay)
		bool DynamicRateAdjustment;

	/**
	 * Factor for calculating the sample rate adjustment.
	 *
	 * If dynamic rate adjustment is enabled, this number is multiplied with the drift
	 * between the audio and media clock (in 100ns ticks) to determine the adjustment.
	 * that is to be multiplied into the current playrate.
	 */
	UPROPERTY(EditAnywhere, Category = "Media", AdvancedDisplay)
		float RateAdjustmentFactor;

	/**
	 * The allowed range of dynamic rate adjustment.
	 *
	 * If dynamic rate adjustment is enabled, and the necessary adjustment
	 * falls outside of this range, audio samples will be dropped.
	 */
	UPROPERTY(EditAnywhere, Category = "Media", AdvancedDisplay)
		FFloatRange RateAdjustmentRange;

public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param ObjectInitializer Initialization parameters.
	 */
	 UMediaSoundPitchShiftComponent(const FObjectInitializer& ObjectInitializer);

	/** Virtual destructor. */
	 ~UMediaSoundPitchShiftComponent();

public:

	/**
	 * Get the attenuation settings based on the current component settings.
	 *
	 * @param OutAttenuationSettings Will contain the attenuation settings, if available.
	 * @return true if attenuation settings were returned, false if attenuation is disabled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaSoundComponent", meta = (DisplayName = "Get Attenuation Settings To Apply", ScriptName = "GetAttenuationSettingsToApply"))
		 bool BP_GetAttenuationSettingsToApply(FSoundAttenuationSettings& OutAttenuationSettings);

	/**
	 * Get the media player that provides the audio samples.
	 *
	 * @return The component's media player, or nullptr if not set.
	 * @see SetMediaPlayer
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaSoundComponent")
		 UMediaPlayer* GetMediaPlayer() const;

	virtual USoundClass* GetSoundClass() override
	{
		if (SoundClass)
		{
			return SoundClass;
		}

		if (const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>())
		{
			if (USoundClass* DefaultSoundClass = AudioSettings->GetDefaultMediaSoundClass())
			{
				return DefaultSoundClass;
			}

			if (USoundClass* DefaultSoundClass = AudioSettings->GetDefaultSoundClass())
			{
				return DefaultSoundClass;
			}
		}

		return nullptr;
	}

	/**
	 * Set the media player that provides the audio samples.
	 *
	 * @param NewMediaPlayer The player to set.
	 * @see GetMediaPlayer
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaSoundComponent")
		 void SetMediaPlayer(UMediaPlayer* NewMediaPlayer);

	/** Turns on spectral analysis of the audio generated in the media sound component. */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaSoundComponent")
		 void SetEnableSpectralAnalysis(bool bInSpectralAnalysisEnabled);

	/** Sets the settings to use for spectral analysis. */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaSoundComponent")
		 void SetSpectralAnalysisSettings(TArray<float> InFrequenciesToAnalyze, EMediaSoundComponentFFTSize InFFTSize = EMediaSoundComponentFFTSize::Medium_512);

	/** Retrieves the spectral data if spectral analysis is enabled. */
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
		 TArray<FMediaSoundComponentSpectralData> GetSpectralData();

	/** Retrieves and normalizes the spectral data if spectral analysis is enabled. */
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
		 TArray<FMediaSoundComponentSpectralData> GetNormalizedSpectralData();

	/** Turns on amplitude envelope following the audio in the media sound component. */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaSoundComponent")
		 void SetEnableEnvelopeFollowing(bool bInEnvelopeFollowing);

	/** Sets the envelope attack and release times (in ms). */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaSoundComponent")
		 void SetEnvelopeFollowingsettings(int32 AttackTimeMsec, int32 ReleaseTimeMsec);

	/** Retrieves the current amplitude envelope. */
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
		 float GetEnvelopeValue() const;

public:

	/** Adds a clock sink so this can be ticked without the world. */
	 void AddClockSink();

	/** Removes the clock sink. */
	 void RemoveClockSink();

	 void UpdatePlayer();

#if WITH_EDITOR
	/**
	 * Set the component's default media player property.
	 *
	 * @param NewMediaPlayer The player to set.
	 * @see SetMediaPlayer
	 */
	 void SetDefaultMediaPlayer(UMediaPlayer* NewMediaPlayer);
#endif

public:

	//~ TAttenuatedComponentVisualizer interface

	 void CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, FBaseAttenuationSettings::AttenuationShapeDetails>& ShapeDetailsMap) const;

public:

	//~ UActorComponent interface

	 virtual void OnRegister() override;
	 virtual void OnUnregister() override;
	 virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:

	//~ USceneComponent interface

	 virtual void Activate(bool bReset = false) override;
	 virtual void Deactivate() override;

public:

	//~ UObject interface
	 virtual void PostLoad() override;

#if WITH_EDITOR
	 virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:

	/**
	 * Get the attenuation settings based on the current component settings.
	 *
	 * @return Attenuation settings, or nullptr if attenuation is disabled.
	 */
	 const FSoundAttenuationSettings* GetSelectedAttenuationSettings() const;

protected:

	//~ USynthComponent interface

	 virtual bool Init(int32& SampleRate) override;

	 virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

protected:

	/**
	 * The media player asset associated with this component.
	 *
	 * This property is meant for design-time convenience. To change the
	 * associated media player at run-time, use the SetMediaPlayer method.
	 *
	 * @see SetMediaPlayer
	 */
	UPROPERTY(EditAnywhere, Category = "Media")
		TObjectPtr<UMediaPlayer> MediaPlayer;

private:

	/** The player's current play rate (cached for use on audio thread). */
	float CachedRate;

	/** The player's current time (cached for use on audio thread). */
	FTimespan CachedTime;

	/** Critical section for synchronizing access to PlayerFacadePtr. */
	FCriticalSection CriticalSection;

	/** The player that is currently associated with this component. */
	TWeakObjectPtr<UMediaPlayer> CurrentPlayer;

	/** The player facade that's currently providing texture samples. */
	TWeakPtr<FMediaPlayerFacade, ESPMode::ThreadSafe> CurrentPlayerFacade;

	/** Adjusts the output sample rate to synchronize audio and media clock. */
	float RateAdjustment;

	/** Audio sample queue. */
	TSharedPtr<FMediaAudioSampleQueue, ESPMode::ThreadSafe> SampleQueue;

	/* Time of last sample played. */
	FTimespan LastPlaySampleTime;

	/** Which frequencies to analyze. */
	TArray<float> FrequenciesToAnalyze;
	/** Spectrum analyzer used for analyzing audio in media. */
	Audio::FSpectrumAnalyzerSettings SpectrumAnalyzerSettings;
	int32 EnvelopeFollowerAttackTime;
	int32 EnvelopeFollowerReleaseTime;

	/** Whether or not spectral analysis is enabled. */
	bool bSpectralAnalysisEnabled;

	/** Whether or not envelope following is enabled. */
	bool bEnvelopeFollowingEnabled;

	/** Holds our clock sink if available. */
	TSharedPtr<FMediaSoundPitchShiftComponentClockSink, ESPMode::ThreadSafe> ClockSink;

	/** Instance of our media sound generator. This is a non-uobject that is used to feed sink audio to a sound source on the audio render thread (or async task). */
	ISoundGeneratorPtr MediaSoundGenerator;
};


// Fill out your copyright notice in the Description page of Project Settings.

//#include "MediaSoundPitchShiftComponent.h"
#include "Component/MediaSoundPitchShiftComponent.h"
#include "Runtime/MediaAssets/Private/MediaAssetsPrivate.h"

#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "IMediaAudioSample.h"
#include "IMediaClock.h"
#include "IMediaClockSink.h"
#include "IMediaModule.h"
#include "IMediaPlayer.h"
#include "MediaAudioResampler.h"
#include "Misc/ScopeLock.h"
#include "Sound/AudioSettings.h"
#include "UObject/UObjectGlobals.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"

//#include UE_INLINE_GENERATED_CPP_BY_NAME(MediaSoundPitchShiftComponent)
DECLARE_FLOAT_COUNTER_STAT(TEXT("MediaUtils MediaSoundPitchShiftComponent Sync"), STAT_MediaUtils_MediaSoundPitchShiftComponentSync, STATGROUP_Media);
DECLARE_FLOAT_COUNTER_STAT(TEXT("MediaUtils MediaSoundPitchShiftComponent SampleTime"), STAT_MediaUtils_MediaSoundPitchShiftComponentSampleTime, STATGROUP_Media);
DECLARE_DWORD_COUNTER_STAT(TEXT("MediaUtils MediaSoundPitchShiftComponent Queued"), STAT_Media_SoundPitchShiftCompQueued, STATGROUP_Media);

CSV_DECLARE_CATEGORY_MODULE_EXTERN(MEDIA_API, MediaStreaming);

/**
 * Clock sink for UMediaSoundComponent.
 */
class FMediaSoundPitchShiftComponentClockSink
	: public IMediaClockSink
{
public:
	FMediaSoundPitchShiftComponentClockSink(UMediaSoundPitchShiftComponent& InOwner)
		: Owner(&InOwner)
	{ }
	virtual ~FMediaSoundPitchShiftComponentClockSink() { }

public:
	virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override
	{
		if (UMediaSoundPitchShiftComponent* OwnerPtr = Owner.Get())
		{
			OwnerPtr->UpdatePlayer();
		}
	}

private:
	TWeakObjectPtr<UMediaSoundPitchShiftComponent> Owner;
};

//-------------------- 自定义内容 ---------------------------
#define PITCH_SHIFT_M_PI 3.14159265358979323846
#define PITCH_SHIFT_MAX_FRAME_LENGTH 8192

double FMediaSoundPitchShiftGenerator::smbAtan2(double x, double y)
{
	double signx;
	if (x > 0.0) signx = 1.0;
	else signx = -1.0;

	if (x == 0.0) return 0.0;
	if (y == 0.0) return signx * PITCH_SHIFT_M_PI / 2.0;

	return FMath::Atan2(x, y);
}

void FMediaSoundPitchShiftGenerator::smbFft(float* fftBuffer, long fftFrameSize, long sign)
{
	float wr, wi, arg, * p1, * p2, temp;
	float tr, ti, ur, ui, * p1r, * p1i, * p2r, * p2i;
	long i, bitm, j, le, le2, k;

	for (i = 2; i < 2 * fftFrameSize - 2; i += 2) {
		for (bitm = 2, j = 0; bitm < 2 * fftFrameSize; bitm <<= 1) {
			if (i & bitm) j++;
			j <<= 1;
		}
		if (i < j) {
			p1 = fftBuffer + i; p2 = fftBuffer + j;
			temp = *p1; *(p1++) = *p2;
			*(p2++) = temp; temp = *p1;
			*p1 = *p2; *p2 = temp;
		}
	}
	for (k = 0, le = 2; k < (long)(log(fftFrameSize) / log(2.) + .5); k++) {
		le <<= 1;
		le2 = le >> 1;
		ur = 1.0;
		ui = 0.0;
		arg = PITCH_SHIFT_M_PI / (le2 >> 1);
		wr = FMath::Cos(arg);
		wi = sign * FMath::Sin(arg);
		for (j = 0; j < le2; j += 2) {
			p1r = fftBuffer + j; p1i = p1r + 1;
			p2r = p1r + le2; p2i = p2r + 1;
			for (i = j; i < 2 * fftFrameSize; i += le) {
				tr = *p2r * ur - *p2i * ui;
				ti = *p2r * ui + *p2i * ur;
				*p2r = *p1r - tr; *p2i = *p1i - ti;
				*p1r += tr; *p1i += ti;
				p1r += le; p1i += le;
				p2r += le; p2i += le;
			}
			tr = ur * wr - ui * wi;
			ui = ur * wi + ui * wr;
			ur = tr;
		}
	}
}

void FMediaSoundPitchShiftGenerator::smbPitchShift(float pitchShift, long numSampsToProcess, long fftFrameSize, long osamp, float sampleRate, float* indata, float* outdata)
{
	static float gInFIFO[PITCH_SHIFT_MAX_FRAME_LENGTH];
	static float gOutFIFO[PITCH_SHIFT_MAX_FRAME_LENGTH];
	static float gFFTworksp[2 * PITCH_SHIFT_MAX_FRAME_LENGTH];
	static float gLastPhase[PITCH_SHIFT_MAX_FRAME_LENGTH / 2 + 1];
	static float gSumPhase[PITCH_SHIFT_MAX_FRAME_LENGTH / 2 + 1];
	static float gOutputAccum[2 * PITCH_SHIFT_MAX_FRAME_LENGTH];
	static float gAnaFreq[PITCH_SHIFT_MAX_FRAME_LENGTH];
	static float gAnaMagn[PITCH_SHIFT_MAX_FRAME_LENGTH];
	static float gSynFreq[PITCH_SHIFT_MAX_FRAME_LENGTH];
	static float gSynMagn[PITCH_SHIFT_MAX_FRAME_LENGTH];
	static long gRover = false, gInit = false;
	double magn, phase, tmp, window, real, imag;
	double freqPerBin, expct;
	long i, k, qpd, index, inFifoLatency, stepSize, fftFrameSize2;

	/* set up some handy variables */
	fftFrameSize2 = fftFrameSize / 2;
	stepSize = fftFrameSize / osamp;
	freqPerBin = sampleRate / (double)fftFrameSize;
	expct = 2. * PITCH_SHIFT_M_PI * (double)stepSize / (double)fftFrameSize;
	inFifoLatency = fftFrameSize - stepSize;
	if (gRover == false) gRover = inFifoLatency;

	/* initialize our static arrays */
	if (gInit == false) {
		FMemory::Memset(gInFIFO, 0, PITCH_SHIFT_MAX_FRAME_LENGTH * sizeof(float));
		FMemory::Memset(gOutFIFO, 0, PITCH_SHIFT_MAX_FRAME_LENGTH * sizeof(float));
		FMemory::Memset(gFFTworksp, 0, 2 * PITCH_SHIFT_MAX_FRAME_LENGTH * sizeof(float));
		FMemory::Memset(gLastPhase, 0, (PITCH_SHIFT_MAX_FRAME_LENGTH / 2 + 1) * sizeof(float));
		FMemory::Memset(gSumPhase, 0, (PITCH_SHIFT_MAX_FRAME_LENGTH / 2 + 1) * sizeof(float));
		FMemory::Memset(gOutputAccum, 0, 2 * PITCH_SHIFT_MAX_FRAME_LENGTH * sizeof(float));
		FMemory::Memset(gAnaFreq, 0, PITCH_SHIFT_MAX_FRAME_LENGTH * sizeof(float));
		FMemory::Memset(gAnaMagn, 0, PITCH_SHIFT_MAX_FRAME_LENGTH * sizeof(float));
		gInit = true;
	}

	/* main processing loop */
	for (i = 0; i < numSampsToProcess; i++) {

		/* As long as we have not yet collected enough data just read in */
		gInFIFO[gRover] = indata[i];
		outdata[i] = gOutFIFO[gRover - inFifoLatency];
		gRover++;

		/* now we have enough data for processing */
		if (gRover >= fftFrameSize) {
			gRover = inFifoLatency;

			/* do windowing and re,im interleave */
			for (k = 0; k < fftFrameSize; k++) {
				window = -.5 * FMath::Cos(2. * PITCH_SHIFT_M_PI * (double)k / (double)fftFrameSize) + .5;
				gFFTworksp[2 * k] = gInFIFO[k] * window;
				gFFTworksp[2 * k + 1] = 0.;
			}


			/* ***************** ANALYSIS ******************* */
			/* do transform */
			smbFft(gFFTworksp, fftFrameSize, -1);

			/* this is the analysis step */
			for (k = 0; k <= fftFrameSize2; k++) {

				/* de-interlace FFT buffer */
				real = gFFTworksp[2 * k];
				imag = gFFTworksp[2 * k + 1];

				/* compute magnitude and phase */
				magn = 2. * FMath::Sqrt(real * real + imag * imag);
				phase = smbAtan2(imag, real);

				/* compute phase difference */
				tmp = phase - gLastPhase[k];
				gLastPhase[k] = phase;

				/* subtract expected phase difference */
				tmp -= (double)k * expct;

				/* map delta phase into +/- Pi interval */
				qpd = tmp / PITCH_SHIFT_M_PI;
				if (qpd >= 0) qpd += qpd & 1;
				else qpd -= qpd & 1;
				tmp -= PITCH_SHIFT_M_PI * (double)qpd;

				/* get deviation from bin frequency from the +/- Pi interval */
				tmp = osamp * tmp / (2. * PITCH_SHIFT_M_PI);

				/* compute the k-th partials' true frequency */
				tmp = (double)k * freqPerBin + tmp * freqPerBin;

				/* store magnitude and true frequency in analysis arrays */
				gAnaMagn[k] = magn;
				gAnaFreq[k] = tmp;

			}

			/* ***************** PROCESSING ******************* */
			/* this does the actual pitch shifting */
			FMemory::Memset(gSynMagn, 0, fftFrameSize * sizeof(float));
			FMemory::Memset(gSynFreq, 0, fftFrameSize * sizeof(float));
			for (k = 0; k <= fftFrameSize2; k++) {
				index = k * pitchShift;
				if (index <= fftFrameSize2) {
					gSynMagn[index] += gAnaMagn[k];
					gSynFreq[index] = gAnaFreq[k] * pitchShift;
				}
			}

			/* ***************** SYNTHESIS ******************* */
			/* this is the synthesis step */
			for (k = 0; k <= fftFrameSize2; k++) {

				/* get magnitude and true frequency from synthesis arrays */
				magn = gSynMagn[k];
				tmp = gSynFreq[k];

				/* subtract bin mid frequency */
				tmp -= (double)k * freqPerBin;

				/* get bin deviation from freq deviation */
				tmp /= freqPerBin;

				/* take osamp into account */
				tmp = 2. * PITCH_SHIFT_M_PI * tmp / osamp;

				/* add the overlap phase advance back in */
				tmp += (double)k * expct;

				/* accumulate delta phase to get bin phase */
				gSumPhase[k] += tmp;
				phase = gSumPhase[k];

				/* get real and imag part and re-interleave */
				gFFTworksp[2 * k] = magn * FMath::Cos(phase);
				gFFTworksp[2 * k + 1] = magn * FMath::Sin(phase);
			}

			/* zero negative frequencies */
			for (k = fftFrameSize + 2; k < 2 * fftFrameSize; k++) gFFTworksp[k] = 0.;

			/* do inverse transform */
			smbFft(gFFTworksp, fftFrameSize, 1);

			/* do windowing and add to output accumulator */
			for (k = 0; k < fftFrameSize; k++) {
				window = -.5 * FMath::Cos(2. * PITCH_SHIFT_M_PI * (double)k / (double)fftFrameSize) + .5;
				gOutputAccum[k] += 2. * window * gFFTworksp[2 * k] / (fftFrameSize2 * osamp);
			}
			for (k = 0; k < stepSize; k++) gOutFIFO[k] = gOutputAccum[k];

			/* shift accumulator */
			FMemory::Memmove(gOutputAccum, gOutputAccum + stepSize, fftFrameSize * sizeof(float));

			/* move input FIFO */
			for (k = 0; k < inFifoLatency; k++) gInFIFO[k] = gInFIFO[k + stepSize];
		}
	}
}

//-------------------- 自定义内容 ---------------------------


FMediaSoundPitchShiftGenerator::FMediaSoundPitchShiftGenerator(FSoundGeneratorParams& InParams)
	: Params(InParams)
{
	// Initialize the settings for the spectrum analyzer
	SpectrumAnalyzer.Init((float)InParams.SampleRate);
	Resampler.Initialize(InParams.NumChannels, InParams.SampleRate);

	Audio::FEnvelopeFollowerInitParams EnvelopeInitParams;
	EnvelopeInitParams.SampleRate = (float)InParams.SampleRate;
	EnvelopeInitParams.NumChannels = 1; //EnvelopeFollower uses mixed down mono buffer 
	EnvelopeFollower.Init(EnvelopeInitParams);

	CachedRate = Params.CachedRate;
	CachedTime = Params.CachedTime;
	LastPlaySampleTime = Params.LastPlaySampleTime;
//-------------------- 自定义内容 ---------------------------
	SampleRate = InParams.SampleRate;
//-------------------- 自定义内容 ---------------------------

}

FMediaSoundPitchShiftGenerator::~FMediaSoundPitchShiftGenerator()
{
//-------------------- 自定义内容 ---------------------------
	if (inputDataPtr)
	{
		free(inputDataPtr);
		inputDataPtr = nullptr;
	}
//-------------------- 自定义内容 ---------------------------
	//UE_LOG(LogMediaAssets, Verbose, TEXT("MediaSoundComponent: FMediaSoundPitchShiftGenerator Destroyed."));
}

void FMediaSoundPitchShiftGenerator::OnEndGenerate()
{
	//UE_LOG(LogMediaAssets, Verbose, TEXT("MediaSoundComponent: OnEndGenerate called."));
	Params.SampleQueue.Reset();
}

void FMediaSoundPitchShiftGenerator::SetCachedData(float InCachedRate, const FTimespan& InCachedTime)
{
	CachedRate = InCachedRate;
	CachedTime = InCachedTime;
}

void FMediaSoundPitchShiftGenerator::SetLastPlaySampleTime(const FTimespan& InLastPlaySampleTime)
{
	LastPlaySampleTime = InLastPlaySampleTime;
}

void FMediaSoundPitchShiftGenerator::SetEnableSpectralAnalysis(bool bInSpectralAnlaysisEnabled)
{
	FScopeLock Lock(&AnalysisCritSect);
	Params.bSpectralAnalysisEnabled = bInSpectralAnlaysisEnabled;
}

void FMediaSoundPitchShiftGenerator::SetEnableEnvelopeFollowing(bool bInEnvelopeFollowingEnabled)
{
	FScopeLock Lock(&AnalysisCritSect);
	Params.bEnvelopeFollowingEnabled = bInEnvelopeFollowingEnabled;
	CurrentEnvelopeValue = 0.0f;
}

void FMediaSoundPitchShiftGenerator::SetSpectrumAnalyzerSettings(Audio::FSpectrumAnalyzerSettings::EFFTSize InFFTSize, const TArray<float>& InFrequenciesToAnalyze)
{
	FScopeLock Lock(&AnalysisCritSect);
	Params.SpectrumAnalyzerSettings.FFTSize = InFFTSize;
	Params.FrequenciesToAnalyze = InFrequenciesToAnalyze;
	SpectrumAnalyzer.SetSettings(Params.SpectrumAnalyzerSettings);
}

void FMediaSoundPitchShiftGenerator::SetEnvelopeFollowingSettings(int32 InAttackTimeMsec, int32 InReleaseTimeMsec)
{
	FScopeLock Lock(&AnalysisCritSect);
	Params.EnvelopeFollowerAttackTime = InAttackTimeMsec;
	Params.EnvelopeFollowerReleaseTime = InReleaseTimeMsec;
	bEnvelopeFollowerSettingsChanged = true;
}

void FMediaSoundPitchShiftGenerator::SetSampleQueue(TSharedPtr<FMediaAudioSampleQueue, ESPMode::ThreadSafe>& InSampleQueue)
{
	FScopeLock Lock(&SampleQueueCritSect);
	Params.SampleQueue = InSampleQueue;

	//UE_LOG(LogMediaAssets, Verbose, TEXT("MediaSoundComponent: SetSampleQueue called with new sample queue."));
}

TArray<FMediaSoundComponentSpectralData> FMediaSoundPitchShiftGenerator::GetSpectralData() const
{
	FScopeLock Lock(&AnalysisCritSect);

	if (Params.bSpectralAnalysisEnabled)
	{
		Audio::FAsyncSpectrumAnalyzerScopeLock AnalyzerBufferLock(&SpectrumAnalyzer);

		TArray<FMediaSoundComponentSpectralData> SpectralData;

		for (float Frequency : Params.FrequenciesToAnalyze)
		{
			FMediaSoundComponentSpectralData Data;
			Data.FrequencyHz = Frequency;
			Data.Magnitude = SpectrumAnalyzer.GetMagnitudeForFrequency(Frequency);
			SpectralData.Add(Data);
		}
		return SpectralData;
	}
	return TArray<FMediaSoundComponentSpectralData>();
}

TArray<FMediaSoundComponentSpectralData> FMediaSoundPitchShiftGenerator::GetNormalizedSpectralData() const
{
	FScopeLock Lock(&AnalysisCritSect);

	if (Params.bSpectralAnalysisEnabled)
	{
		Audio::FAsyncSpectrumAnalyzerScopeLock AnalyzerBufferLock(&SpectrumAnalyzer);

		TArray<FMediaSoundComponentSpectralData> SpectralData;

		for (float Frequency : Params.FrequenciesToAnalyze)
		{
			FMediaSoundComponentSpectralData Data;
			Data.FrequencyHz = Frequency;
			Data.Magnitude = SpectrumAnalyzer.GetNormalizedMagnitudeForFrequency(Frequency);
			SpectralData.Add(Data);
		}

		return SpectralData;
	}
	return TArray<FMediaSoundComponentSpectralData>();
}

//-------------------- 自定义内容 ---------------------------
void FMediaSoundPitchShiftGenerator::UpdatePitchShift(float InNewPitchSemitones)
{
	pitchSemitones = InNewPitchSemitones;
	pitchShiftFrameRatio = FMath::Pow(2.0f, pitchSemitones / 12.0f);
}


int32 FMediaSoundPitchShiftGenerator::OnGenerateAudio_Origin(float* OutAudio, int32 NumSamples)
{
	//CSV_SCOPED_TIMING_STAT(MediaStreaming, FMediaSoundPitchShiftGenerator_OnGenerateAudio);

	int32 InitialSyncOffset = 0;

	TSharedPtr<FMediaAudioSampleQueue, ESPMode::ThreadSafe> PinnedSampleQueue;
	// Make sure we don't swap the sample queue ptr while we're generating
	{
		FScopeLock Lock(&SampleQueueCritSect);
		PinnedSampleQueue = Params.SampleQueue;
	}

	const float Rate = CachedRate.Load();

	// 	// We have an input queue and are actively playing?
	if (PinnedSampleQueue.IsValid() && (Rate != 0.0f))
	{
		const FTimespan Time = CachedTime.Load();

		{
			const uint32 FramesRequested = uint32(NumSamples / Params.NumChannels);
			uint32 JumpFrame = MAX_uint32;
			FMediaTimeStamp OutTime = FMediaTimeStamp(FTimespan::Zero());
			uint32 FramesWritten = Resampler.Generate(OutAudio, OutTime, FramesRequested, Rate, Time, *PinnedSampleQueue, JumpFrame);
			if (FramesWritten == 0)
			{
				return NumSamples; // no samples available
			}

			// Fill in any gap left as we didn't have enough data
			if (FramesWritten < FramesRequested)
			{
				memset(OutAudio + FramesWritten * Params.NumChannels, 0, (NumSamples - FramesWritten * Params.NumChannels) * sizeof(float));
			}

			// Update audio time
			LastPlaySampleTime = OutTime.Time;
			PinnedSampleQueue->SetAudioTime(FMediaTimeStampSample(OutTime, FPlatformTime::Seconds()));

			SET_FLOAT_STAT(STAT_MediaUtils_MediaSoundPitchShiftComponentSampleTime, OutTime.Time.GetTotalSeconds());
			SET_DWORD_STAT(STAT_Media_SoundPitchShiftCompQueued, PinnedSampleQueue->Num());
		}

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

		if (Params.bSpectralAnalysisEnabled || Params.bEnvelopeFollowingEnabled)
		{
			float* BufferToUseForAnalysis = nullptr;
			int32 NumFrames = NumSamples;

			if (Params.NumChannels == 2)
			{
				NumFrames = NumSamples / 2;

				// Use the scratch buffer to sum the audio to mono
				AudioScratchBuffer.Reset();
				AudioScratchBuffer.AddUninitialized(NumFrames);
				BufferToUseForAnalysis = AudioScratchBuffer.GetData();
				int32 SampleIndex = 0;
				for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex, SampleIndex += Params.NumChannels)
				{
					BufferToUseForAnalysis[FrameIndex] = 0.5f * (OutAudio[SampleIndex] + OutAudio[SampleIndex + 1]);
				}
			}
			else
			{
				BufferToUseForAnalysis = OutAudio;
			}

			if (Params.bSpectralAnalysisEnabled)
			{
				SpectrumAnalyzer.PushAudio(BufferToUseForAnalysis, NumFrames);
				SpectrumAnalyzer.PerformAsyncAnalysisIfPossible(true);
			}

			{
				FScopeLock ScopeLock(&AnalysisCritSect);
				if (Params.bEnvelopeFollowingEnabled)
				{
					if (bEnvelopeFollowerSettingsChanged)
					{
						EnvelopeFollower.SetAttackTime((float)Params.EnvelopeFollowerAttackTime);
						EnvelopeFollower.SetReleaseTime((float)Params.EnvelopeFollowerReleaseTime);

						bEnvelopeFollowerSettingsChanged = false;
					}

					EnvelopeFollower.ProcessAudio(BufferToUseForAnalysis, NumFrames);

					const TArray<float>& EnvelopeValues = EnvelopeFollower.GetEnvelopeValues();
					if (ensure(EnvelopeValues.Num() > 0))
					{
						CurrentEnvelopeValue = FMath::Clamp(EnvelopeValues[0], 0.f, 1.f);
					}
					else
					{
						CurrentEnvelopeValue = 0.f;
					}
				}
			}
		}
	}
	else
	{
		Resampler.Flush();

		LastPlaySampleTime = FTimespan::MinValue();
	}
	return NumSamples;
}
int32 FMediaSoundPitchShiftGenerator::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	NumSamples = OnGenerateAudio_Origin(OutAudio, NumSamples);
	
	if (NumSamples == 0 || pitchSemitones == 0.0f)
		return NumSamples;

	if (inputDataPtr == nullptr)
	{
		inputBufferSize = NumSamples * 2;
		inputDataPtr = (float*)malloc(inputBufferSize * sizeof(float));
	}
	if (inputBufferSize < NumSamples)
	{
		if (inputDataPtr)
		{
			free(inputDataPtr);
			inputDataPtr = nullptr;
		}
		inputBufferSize = NumSamples * 2;
		inputDataPtr = (float*)malloc(inputBufferSize * sizeof(float));
	}
	
	FMemory::Memcpy(inputDataPtr, OutAudio, NumSamples * sizeof(float));
	smbPitchShift(pitchShiftFrameRatio, NumSamples, NumSamples, 4, SampleRate, inputDataPtr, OutAudio);
	//UE_LOG(LogTemp, Warning, TEXT("OnGenerateAudio %d %f %f %d"), SampleRate, pitchSemitones, pitchShiftFrameRatio, NumSamples);
	
	return NumSamples;
}


static const int32 MaxAudioInputSamples = 8;	// accept at most these many samples into our input queue

/* UMediaSoundPitchShiftComponent structors
 *****************************************************************************/

UMediaSoundPitchShiftComponent::UMediaSoundPitchShiftComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Channels(EMediaSoundChannels::Stereo)
	, DynamicRateAdjustment(false)
	, RateAdjustmentFactor(0.00000001f)
	, RateAdjustmentRange(FFloatRange(0.995f, 1.005f))
	, CachedRate(0.0f)
	, CachedTime(FTimespan::Zero())
	, RateAdjustment(1.0f)
	, LastPlaySampleTime(FTimespan::MinValue())
	, EnvelopeFollowerAttackTime(10)
	, EnvelopeFollowerReleaseTime(100)
	, bSpectralAnalysisEnabled(false)
	, bEnvelopeFollowingEnabled(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;

#if PLATFORM_MAC
	PreferredBufferLength = 4 * 1024; // increase buffer callback size on macOS to prevent underruns
#endif

#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
}


UMediaSoundPitchShiftComponent::~UMediaSoundPitchShiftComponent()
{
	RemoveClockSink();
}


/* UMediaSoundPitchShiftComponent interface
 *****************************************************************************/

bool UMediaSoundPitchShiftComponent::BP_GetAttenuationSettingsToApply(FSoundAttenuationSettings& OutAttenuationSettings)
{
	const FSoundAttenuationSettings* SelectedAttenuationSettings = GetSelectedAttenuationSettings();

	if (SelectedAttenuationSettings == nullptr)
	{
		return false;
	}

	OutAttenuationSettings = *SelectedAttenuationSettings;

	return true;
}


UMediaPlayer* UMediaSoundPitchShiftComponent::GetMediaPlayer() const
{
	return CurrentPlayer.Get();
}


void UMediaSoundPitchShiftComponent::SetMediaPlayer(UMediaPlayer* NewMediaPlayer)
{
	CurrentPlayer = NewMediaPlayer;
}

#if WITH_EDITOR

void UMediaSoundPitchShiftComponent::SetDefaultMediaPlayer(UMediaPlayer* NewMediaPlayer)
{
	MediaPlayer = NewMediaPlayer;
	CurrentPlayer = MediaPlayer;
}

#endif


void UMediaSoundPitchShiftComponent::UpdatePlayer()
{
	UMediaPlayer* CurrentPlayerPtr = CurrentPlayer.Get();
	if (CurrentPlayerPtr == nullptr)
	{
		CachedRate = 0.0f;
		CachedTime = FTimespan::Zero();

		FScopeLock Lock(&CriticalSection);
		SampleQueue.Reset();
		MediaSoundGenerator.Reset();
		return;
	}

	// create a new sample queue if the player changed
	TSharedRef<FMediaPlayerFacade, ESPMode::ThreadSafe> PlayerFacade = CurrentPlayerPtr->GetPlayerFacade();

	// We have some audio decoders which are running with a limited amount of pre-allocated audio sample packets. 
	// When the audio packets are not consumed in the FMediaSoundPitchShiftGenerator::OnGenerateAudio method below, these packets are not 
	// returned to the decoder which then cannot produce more audio samples. 
	//
	// The FMediaSoundPitchShiftGenerator::OnGenerateAudio is only called when our parent USynthComponent it active and
	// this is controlled by USynthComponent::Start() and USynthComponent::Stop(). We are tracking a state change here.
	if (PlayerFacade != CurrentPlayerFacade)
	{
		if (IsActive())
		{
			const auto NewSampleQueue = MakeShared<FMediaAudioSampleQueue, ESPMode::ThreadSafe>(MaxAudioInputSamples);
			PlayerFacade->AddAudioSampleSink(NewSampleQueue);
			{
				FScopeLock Lock(&CriticalSection);
				SampleQueue = NewSampleQueue;
				if (MediaSoundGenerator.IsValid())
				{
					static_cast<FMediaSoundPitchShiftGenerator*>(MediaSoundGenerator.Get())->SetSampleQueue(SampleQueue);
				}
			}
			CurrentPlayerFacade = PlayerFacade;
		}
	}
	else
	{
		// Here, we have a CurrentPlayerFacade set which means are also have a valid FMediaAudioSampleQueue set
		// We need to check for deactivation as it seems there is not callback scheduled when USynthComponent::Stop() is called.
		if (!IsActive())
		{
			FScopeLock Lock(&CriticalSection);
			SampleQueue.Reset();
			CurrentPlayerFacade.Reset();
		}
	}

	// caching play rate and time for audio thread (eventual consistency is sufficient)
	CachedRate = PlayerFacade->GetRate();
	CachedTime = PlayerFacade->GetTime();

	if (MediaSoundGenerator.IsValid())
	{
		FMediaSoundPitchShiftGenerator* MediaGen = static_cast<FMediaSoundPitchShiftGenerator*>(MediaSoundGenerator.Get());

		// The play time is derived from the media sound generator's OnGenerate callback
		LastPlaySampleTime = MediaGen->GetLastPlayTime();

		MediaGen->SetCachedData(CachedRate, CachedTime);
		PlayerFacade->SetLastAudioRenderedSampleTime(LastPlaySampleTime);
	}
	else
	{
		PlayerFacade->SetLastAudioRenderedSampleTime(FTimespan::MinValue());
	}
}


/* TAttenuatedComponentVisualizer interface
 *****************************************************************************/

void UMediaSoundPitchShiftComponent::CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, FBaseAttenuationSettings::AttenuationShapeDetails>& ShapeDetailsMap) const
{
	const FSoundAttenuationSettings* SelectedAttenuationSettings = GetSelectedAttenuationSettings();

	if (SelectedAttenuationSettings != nullptr)
	{
		SelectedAttenuationSettings->CollectAttenuationShapesForVisualization(ShapeDetailsMap);
	}
}


/* UActorComponent interface
 *****************************************************************************/

void UMediaSoundPitchShiftComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITORONLY_DATA
	if (SpriteComponent != nullptr)
	{
		SpriteComponent->SpriteInfo.Category = TEXT("Sounds");
		SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT("SpriteCategory", "Sounds", "Sounds");

		if (bAutoActivate)
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorResources/AudioIcons/S_AudioComponent_AutoActivate.S_AudioComponent_AutoActivate")));
		}
		else
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorResources/AudioIcons/S_AudioComponent.S_AudioComponent")));
		}
	}
#endif
}

void UMediaSoundPitchShiftComponent::OnUnregister()
{
	{
		FScopeLock Lock(&CriticalSection);
		SampleQueue.Reset();
	}
	CurrentPlayerFacade.Reset();
	MediaSoundGenerator.Reset();
	Super::OnUnregister();
}


void UMediaSoundPitchShiftComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdatePlayer();
}


/* USceneComponent interface
 *****************************************************************************/

void UMediaSoundPitchShiftComponent::Activate(bool bReset)
{
	if (bReset || ShouldActivate())
	{
		SetComponentTickEnabled(true);
	}

	Super::Activate(bReset);
}


void UMediaSoundPitchShiftComponent::Deactivate()
{
	if (!ShouldActivate())
	{
		SetComponentTickEnabled(false);
		{
			FScopeLock Lock(&CriticalSection);
			SampleQueue.Reset();
		}
		CurrentPlayerFacade.Reset();
		MediaSoundGenerator.Reset();
	}
	Super::Deactivate();
}


/* UObject interface
 *****************************************************************************/


void UMediaSoundPitchShiftComponent::PostLoad()
{
	Super::PostLoad();

	CurrentPlayer = MediaPlayer;
}


#if WITH_EDITOR

void UMediaSoundPitchShiftComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName MediaPlayerName = GET_MEMBER_NAME_CHECKED(UMediaSoundPitchShiftComponent, MediaPlayer);

	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	if (PropertyThatChanged != nullptr)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();

		if (PropertyName == MediaPlayerName)
		{
			CurrentPlayer = MediaPlayer;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif //WITH_EDITOR


/* USynthComponent interface
 *****************************************************************************/

bool UMediaSoundPitchShiftComponent::Init(int32& SampleRate)
{
	Super::Init(SampleRate);

	if (Channels == EMediaSoundChannels::Mono)
	{
		NumChannels = 1;
	}
	else if (Channels == EMediaSoundChannels::Stereo)
	{
		NumChannels = 2;
	}
	else
	{
		NumChannels = 8;
	}

	return true;
}

void UMediaSoundPitchShiftComponent::SetEnableSpectralAnalysis(bool bInSpectralAnalysisEnabled)
{
	bSpectralAnalysisEnabled = bInSpectralAnalysisEnabled;
	if (MediaSoundGenerator.IsValid())
	{
		FMediaSoundPitchShiftGenerator* MediaGen = static_cast<FMediaSoundPitchShiftGenerator*>(MediaSoundGenerator.Get());
		MediaGen->SetEnableSpectralAnalysis(bSpectralAnalysisEnabled);
	}
}

void UMediaSoundPitchShiftComponent::SetSpectralAnalysisSettings(TArray<float> InFrequenciesToAnalyze, EMediaSoundComponentFFTSize InFFTSize)
{
	Audio::FSpectrumAnalyzerSettings::EFFTSize SpectrumAnalyzerSize;

	switch (InFFTSize)
	{
	case EMediaSoundComponentFFTSize::Min_64:
		SpectrumAnalyzerSize = Audio::FSpectrumAnalyzerSettings::EFFTSize::Min_64;
		break;

	case EMediaSoundComponentFFTSize::Small_256:
		SpectrumAnalyzerSize = Audio::FSpectrumAnalyzerSettings::EFFTSize::Small_256;
		break;

	default:
	case EMediaSoundComponentFFTSize::Medium_512:
		SpectrumAnalyzerSize = Audio::FSpectrumAnalyzerSettings::EFFTSize::Medium_512;
		break;

	case EMediaSoundComponentFFTSize::Large_1024:
		SpectrumAnalyzerSize = Audio::FSpectrumAnalyzerSettings::EFFTSize::Large_1024;
		break;
	}

	SpectrumAnalyzerSettings.FFTSize = SpectrumAnalyzerSize;
	FrequenciesToAnalyze = InFrequenciesToAnalyze;

	if (MediaSoundGenerator.IsValid())
	{
		FMediaSoundPitchShiftGenerator* MediaGen = static_cast<FMediaSoundPitchShiftGenerator*>(MediaSoundGenerator.Get());
		MediaGen->SetSpectrumAnalyzerSettings(SpectrumAnalyzerSize, InFrequenciesToAnalyze);
	}
}

TArray<FMediaSoundComponentSpectralData> UMediaSoundPitchShiftComponent::GetSpectralData()
{
	if (bSpectralAnalysisEnabled)
	{
		if (MediaSoundGenerator.IsValid())
		{
			FMediaSoundPitchShiftGenerator* MediaGen = static_cast<FMediaSoundPitchShiftGenerator*>(MediaSoundGenerator.Get());
			return MediaGen->GetSpectralData();
		}
	}
	// Empty array if spectrum analysis is not implemented
	return TArray<FMediaSoundComponentSpectralData>();
}

TArray<FMediaSoundComponentSpectralData> UMediaSoundPitchShiftComponent::GetNormalizedSpectralData()
{
	if (bSpectralAnalysisEnabled)
	{
		if (MediaSoundGenerator.IsValid())
		{
			FMediaSoundPitchShiftGenerator* MediaGen = static_cast<FMediaSoundPitchShiftGenerator*>(MediaSoundGenerator.Get());
			return MediaGen->GetNormalizedSpectralData();
		}
	}
	// Empty array if spectrum analysis is not implemented
	return TArray<FMediaSoundComponentSpectralData>();
}

void UMediaSoundPitchShiftComponent::SetEnableEnvelopeFollowing(bool bInEnvelopeFollowing)
{
	bEnvelopeFollowingEnabled = bInEnvelopeFollowing;

	if (MediaSoundGenerator.IsValid())
	{
		FMediaSoundPitchShiftGenerator* MediaGen = static_cast<FMediaSoundPitchShiftGenerator*>(MediaSoundGenerator.Get());
		MediaGen->SetEnableEnvelopeFollowing(bInEnvelopeFollowing);
	}
}

void UMediaSoundPitchShiftComponent::SetEnvelopeFollowingsettings(int32 AttackTimeMsec, int32 ReleaseTimeMsec)
{
	EnvelopeFollowerAttackTime = AttackTimeMsec;
	EnvelopeFollowerReleaseTime = ReleaseTimeMsec;

	if (MediaSoundGenerator.IsValid())
	{
		FMediaSoundPitchShiftGenerator* MediaGen = static_cast<FMediaSoundPitchShiftGenerator*>(MediaSoundGenerator.Get());
		MediaGen->SetEnvelopeFollowingSettings(EnvelopeFollowerAttackTime, EnvelopeFollowerReleaseTime);
	}
}

float UMediaSoundPitchShiftComponent::GetEnvelopeValue() const
{
	if (MediaSoundGenerator.IsValid())
	{
		FMediaSoundPitchShiftGenerator* MediaGen = static_cast<FMediaSoundPitchShiftGenerator*>(MediaSoundGenerator.Get());
		return MediaGen->GetCurrentEnvelopeValue();
	}

	return 0.0f;
}

void UMediaSoundPitchShiftComponent::AddClockSink()
{
	if (!ClockSink.IsValid())
	{
		IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

		if (MediaModule != nullptr)
		{
			ClockSink = MakeShared<FMediaSoundPitchShiftComponentClockSink, ESPMode::ThreadSafe>(*this);
			MediaModule->GetClock().AddSink(ClockSink.ToSharedRef());
		}
	}
}

void UMediaSoundPitchShiftComponent::RemoveClockSink()
{
	if (ClockSink.IsValid())
	{
		IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

		if (MediaModule != nullptr)
		{
			MediaModule->GetClock().RemoveSink(ClockSink.ToSharedRef());
		}

		ClockSink.Reset();
	}
}

/* UMediaSoundPitchShiftComponent implementation
 *****************************************************************************/

const FSoundAttenuationSettings* UMediaSoundPitchShiftComponent::GetSelectedAttenuationSettings() const
{
	if (bOverrideAttenuation)
	{
		return &AttenuationOverrides;
	}

	if (AttenuationSettings != nullptr)
	{
		return &AttenuationSettings->Attenuation;
	}

	return nullptr;
}

//-------------------- 自定义内容 ---------------------------
void UMediaSoundPitchShiftComponent::UpdatePitchShift(float InNewPitchSemitones)
{
	if (MediaSoundGenerator.IsValid())
	{
		FMediaSoundPitchShiftGenerator* MediaGen = static_cast<FMediaSoundPitchShiftGenerator*>(MediaSoundGenerator.Get());
		MediaGen->UpdatePitchShift(InNewPitchSemitones);
	}
}

ISoundGeneratorPtr UMediaSoundPitchShiftComponent::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	FMediaSoundPitchShiftGenerator::FSoundGeneratorParams Params;
	Params.SampleRate = (int32)InParams.SampleRate;
	Params.NumChannels = InParams.NumChannels;
	Params.SampleQueue = SampleQueue;

	Params.bSpectralAnalysisEnabled = bSpectralAnalysisEnabled;
	Params.bEnvelopeFollowingEnabled = bEnvelopeFollowingEnabled;
	Params.EnvelopeFollowerAttackTime = EnvelopeFollowerAttackTime;
	Params.EnvelopeFollowerReleaseTime = EnvelopeFollowerReleaseTime;
	Params.SpectrumAnalyzerSettings = SpectrumAnalyzerSettings;
	Params.FrequenciesToAnalyze = FrequenciesToAnalyze;

	Params.CachedRate = CachedRate;
	Params.CachedTime = CachedTime;
	Params.LastPlaySampleTime = LastPlaySampleTime;

	return MediaSoundGenerator = ISoundGeneratorPtr(new FMediaSoundPitchShiftGenerator(Params));
}
//-------------------- 自定义内容 ---------------------------

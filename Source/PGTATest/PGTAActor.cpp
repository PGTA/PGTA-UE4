// Fill out your copyright notice in the Description page of Project Settings.

#include "PGTATest.h"
#include "PGTAActor.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "AudioDecompress.h"
#include "Audio.h"
#include "Sound/AudioVolume.h"
#include "AudioDevice.h"
#include "Sound/SoundWaveStreaming.h"

// Sets default values
APGTAActor::APGTAActor(const FObjectInitializer& ObjectInitializer) :
PrimaryTrack(),
SecondaryTrack(),
VolumeMultiplier(1.0f),
m_audioComponent(NULL),
m_streamingWave(NULL),
m_soundWaveArray(),
m_pgtaDevice(),
m_pgtaContext(),
m_primaryTrack(NULL),
m_secondaryTrack(NULL)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

APGTAActor::~APGTAActor()
{
}

// Called when the game starts or when spawned
void APGTAActor::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("PGTA initialization start"));
	USoundWaveStreaming* streamingWave = NewObject<USoundWaveStreaming>();
	streamingWave->SampleRate = 44100;
	streamingWave->NumChannels = 1;
	streamingWave->Duration = INDEFINITELY_LOOPING_DURATION;
	streamingWave->bLooping = false;
	streamingWave->OnSoundWaveStreamingUnderflow =
		FOnSoundWaveStreamingUnderflow::CreateUObject(this, &APGTAActor::StreamingWaveUnderflow);
	m_streamingWave = streamingWave;

	m_audioComponent = NewObject<UAudioComponent>();
	//audioComponent->AttachParent = RootComponent;
	//audioComponent->bStopWhenOwnerDestroyed = false;

	if (!m_pgtaDevice.Initialize())
	{
		UE_LOG(LogTemp, Warning, TEXT("PGTA initialization failed"));
	}

	PGTAConfig config = PGTAConfig();
	config.audioDesc.samplesPerSecond = 44100;
	config.audioDesc.bytesPerSample = 2;
	config.audioDesc.channels = 1;
	config.mixAhead = 0.0f;
	m_pgtaContext = m_pgtaDevice.CreateContext(config);

	m_primaryTrack = LoadPGTATrack(PrimaryTrack);
	m_secondaryTrack = LoadPGTATrack(SecondaryTrack);
	m_thirdTrack = LoadPGTATrack(ThirdTrack);
	m_fourthTrack = LoadPGTATrack(FourthTrack);
	if (m_primaryTrack)
	{
		m_pgtaContext.BindTrack(m_primaryTrack);

		m_audioComponent->SetSound(m_streamingWave);
		m_audioComponent->Play();
	}
}

void APGTAActor::BeginDestroy()
{
	/*
	if (m_audioComponent)
	{
		m_audioComponent->Stop();
		m_audioComponent->SetSound(NULL);
		//m_audioComponent = NULL;
	}
	if (m_streamingWave)
	{
		m_streamingWave->ResetAudio();
		//m_streamingWave = NULL;
	}

	m_pgtaDevice.DestroyContext(m_pgtaContext);
	if (m_secondaryTrack)
	{
		m_pgtaDevice.FreeTracks(1, &m_secondaryTrack);
		//m_secondaryTrack = NULL;
	}
	if (m_primaryTrack)
	{
		m_pgtaDevice.FreeTracks(1, &m_primaryTrack);
		//m_primaryTrack = NULL;
	}
	m_pgtaDevice.Destroy();

	const int32 numSoundWaves = m_soundWaveArray.Num();
	for (int32 i = 0; i < numSoundWaves; ++i)
	{
		m_soundWaveArray[i]->FreeResources();
	}
	m_soundWaveArray.Empty();
	*/
	Super::BeginDestroy();
}

// Called every frame
void APGTAActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

USoundWave* APGTAActor::DecompressSoundWave(USoundWave* soundWave)
{
	FAudioDevice* audioDevice = GEngine ? GEngine->GetAudioDevice() : NULL;
	if (audioDevice && soundWave)
	{
		if (!soundWave->RawPCMData)
		{
			//audioDevice->Precache(soundWave, true, true);
			soundWave->DecompressionType = DTYPE_Native;
			const FName runtimeFormat = audioDevice->GetRuntimeFormat(soundWave);
			soundWave->InitAudioResource(runtimeFormat);
			FAsyncAudioDecompress TempDecompress(soundWave);
			TempDecompress.StartSynchronousTask();
			static FName NAME_OGG(TEXT("OGG"));
			soundWave->bDecompressedFromOgg = (runtimeFormat == NAME_OGG);
		}
		return soundWave;
	}
	return NULL;
}

HPGTATrack APGTAActor::LoadPGTATrack(const FFilePath& track)
{
	UE_LOG(LogTemp, Warning, TEXT("PGTA load track"));
	TArray<uint8> trackData;
	if (!FFileHelper::LoadFileToArray(trackData, *(track.FilePath)))
	{
		UE_LOG(LogTemp, Warning, TEXT("Error loading track (%s)"), *(track.FilePath));
		return NULL;
	}
	trackData.Add('\0');

	HPGTATrack pgtaTrack = NULL;
	const char* source = reinterpret_cast<const char*>(trackData.GetData());
	const size_t length = static_cast<size_t>(trackData.Num());
	if (m_pgtaDevice.CreateTracks(1, &source, &length, &pgtaTrack) <= 0 || !pgtaTrack)
	{
		UE_LOG(LogTemp, Warning, TEXT("Error creating track"));
		return NULL;
	}

	const PGTATrackData pgtaTrackData = pgtaGetTrackData(pgtaTrack);
	if (pgtaTrackData.numSamples == 0 || !pgtaTrackData.samples)
	{
		UE_LOG(LogTemp, Warning, TEXT("Error, no samples in track data"));
		m_pgtaDevice.FreeTracks(1, &pgtaTrack);
		return NULL;
	}

	const int32 oldSampleCount = m_soundWaveArray.Num();
	const uint16 numSamples = pgtaTrackData.numSamples;
	const PGTASampleData* pgtaSamples = pgtaTrackData.samples;
	for (uint32 i = 0; i < numSamples; ++i)
	{
		const PGTASampleData* sample = (pgtaSamples + i);
		FString defaultFile(sample->defaultFile);
		FString formattedFile = FPaths::GetBaseFilename(defaultFile, false) +
			"." +
			FPaths::GetBaseFilename(defaultFile);
		USoundWave* soundWave = LoadObject<USoundWave>(NULL, *formattedFile,
			NULL, LOAD_None, NULL);
		if (!soundWave)
		{
			UE_LOG(LogTemp, Warning, TEXT("Error loading sample (%s)"), *formattedFile);
			continue;
		}

		if (!DecompressSoundWave(soundWave))
		{
			UE_LOG(LogTemp, Warning, TEXT("Error decompressing sample (%s)"), *formattedFile);
			continue;
		}

		//const int16* samples = reinterpret_cast<int16*>(soundWave->RawPCMData);
		//const int32 numSamples = soundWave->RawPCMDataSize * sizeof(uint8) / sizeof(int16);

		PGTA::BindTrackSample(pgtaTrack, sample->id, soundWave->RawPCMData, soundWave->RawPCMDataSize);
		m_soundWaveArray.Add(soundWave);
	}

	if (oldSampleCount >= m_soundWaveArray.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("Error track sample loading failed"));
		m_pgtaDevice.FreeTracks(1, &pgtaTrack);
		return NULL;
	}
	return pgtaTrack;
}

void APGTAActor::StreamingWaveUnderflow(USoundWaveStreaming* InStreamingWave,
	int32 SamplesRequired)
{
	check(InStreamingWave == m_streamingWave);
	m_audioComponent->SetVolumeMultiplier(VolumeMultiplier);
	UE_LOG(LogTemp, Warning, TEXT("PGTA stream underflow"));
	PGTABuffer output = m_pgtaContext.Update(((float)SamplesRequired / InStreamingWave->SampleRate) + 0.1f);
	//UE_LOG(LogTemp, Warning, TEXT("Tick %f %d"), DeltaSeconds, output.numSamples);
	if (output.numSamples >= SamplesRequired && output.samples)
	{
		const uint8* audioData = reinterpret_cast<const uint8*>(output.samples);
		InStreamingWave->QueueAudio(audioData, output.numSamples * 2);
	}
	else
	{
		m_audioComponent->Stop();
		UE_LOG(LogTemp, Warning, TEXT("PGTA playback stopped"));

		TArray<int16> data;
		data.AddZeroed(SamplesRequired);
		InStreamingWave->QueueAudio(reinterpret_cast<const uint8*>(data.GetData()),
			SamplesRequired);
	}
}

void APGTAActor::TrackTransition(AActor* actor) {
	transitionIndex++; 
	if (transitionIndex >= 4) {
		transitionIndex = 0;
	}
	UE_LOG(LogTemp, Warning, TEXT("PGTA transitioning track"));

	if (transitionIndex == 0) {
		m_pgtaContext.Transition(m_primaryTrack, 1.0f, 10.0f);
	}
	else if (transitionIndex == 1) {
		m_pgtaContext.Transition(m_secondaryTrack, 1.0f, 10.0f);
	}
	else if (transitionIndex == 2) {
		m_pgtaContext.Transition(m_thirdTrack, 1.0f, 10.0f);
	}
	else if (transitionIndex == 3) {
		m_pgtaContext.Transition(m_fourthTrack, 1.0f, 10.0f);
	}
}
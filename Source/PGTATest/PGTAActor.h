// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "Sound/SoundWaveStreaming.h"
#include <akPGTA.h>
#include "PGTAActor.generated.h"

UCLASS()
class PGTATEST_API APGTAActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
    APGTAActor(const FObjectInitializer& ObjectInitializer);
    virtual ~APGTAActor();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

    virtual void BeginDestroy() override;
	
	// Called every frame
	virtual void Tick( float DeltaSeconds ) override;

public:
    UPROPERTY(EditAnywhere, Category = "PGTA")
    FFilePath PrimaryTrack;

    UPROPERTY(EditAnywhere, Category = "PGTA")
    FFilePath SecondaryTrack;

    UPROPERTY(EditAnywhere, Category = "PGTA")
    float VolumeMultiplier;

private:
    static USoundWave* DecompressSoundWave(USoundWave* soundWave);

    HPGTATrack LoadPGTATrack(const FFilePath& track);

    void StreamingWaveUnderflow(USoundWaveStreaming* InStreamingWave, int32 SamplesRequired);

private:
    UAudioComponent* m_audioComponent;
    USoundWaveStreaming* m_streamingWave;

    TArray<USoundWave*> m_soundWaveArray;

    PGTA::PGTADevice m_pgtaDevice;
    PGTA::PGTAContext m_pgtaContext;

    HPGTATrack m_primaryTrack;
    HPGTATrack m_secondaryTrack;
};

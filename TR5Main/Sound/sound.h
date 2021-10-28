#pragma once

#include <bass.h>
#include <bass_fx.h>
#include "control/control.h"
#include "sound_effects.h"

#define SFX_ALWAYS 2

#define SOUND_BASS_UNITS			 1.0f / 1024.0f	// TR->BASS distance unit coefficient
#define SOUND_MAXVOL_RADIUS			 1024.0f		// Max. volume hearing distance
#define SOUND_OMNIPRESENT_ORIGIN     Vector3(1.17549e-038f, 1.17549e-038f, 1.17549e-038f)
#define SOUND_MAX_SAMPLES			 8192 // Original was 1024, reallocate original 3-dword DX handle struct to just 1-dword memory pointer
#define SOUND_MAX_CHANNELS			 32 // Original was 24, reallocate original 36-byte struct with 24-byte SoundEffectSlot struct
#define SOUND_LEGACY_SOUNDMAP_SIZE	 450
#define SOUND_NEW_SOUNDMAP_MAX_SIZE	 4096
#define SOUND_LEGACY_TRACKTABLE_SIZE 136
#define SOUND_FLAG_NO_PAN			 (1<<12)	// Unused flag
#define SOUND_FLAG_RND_PITCH		 (1<<13)
#define SOUND_FLAG_RND_GAIN			 (1<<14)
#define SOUND_MAX_PITCH_CHANGE		 0.09f
#define SOUND_MAX_GAIN_CHANGE		 0.0625f
#define SOUND_32BIT_SILENCE_LEVEL	 4.9e-04f
#define SOUND_SAMPLE_FLAGS			 (BASS_SAMPLE_MONO | BASS_SAMPLE_FLOAT)
#define SOUND_XFADETIME_BGM			 5000
#define SOUND_XFADETIME_BGM_START	 1500
#define SOUND_XFADETIME_ONESHOT		 200
#define SOUND_XFADETIME_CUTSOUND	 100
#define SOUND_XFADETIME_HIJACKSOUND	 50
#define SOUND_BGM_DAMP_COEFFICIENT	 0.6f

#define TRACKS_PREFIX				"Audio\\%s.%s"

enum class SOUNDTRACK_PLAYTYPE
{
	OneShot,
	BGM,
	Count
};

enum class SOUND_PLAYCONDITION
{
	LandAndWater = 0,
	Land  = (1 << 14),
	Water = (2 << 14)
};

enum class SOUND_PLAYTYPE
{
	Normal,
	Wait,
	Restart,
	Looped
};

enum class REVERB_TYPE
{
	Outside,  // 0x00   no reverberation
	Small,	  // 0x01   little reverberation
	Medium,   // 0x02
	Large,	  // 0x03
	Pipe,	  // 0x04   highest reverberation, almost never used
	Count
};

enum class SOUND_STATE
{
	Idle,
	Ending,
	Ended
};

enum class SOUND_FILTER
{
	Reverb,
	Compressor,
	Lowpass,
	Count
};

struct SoundEffectSlot
{
	SOUND_STATE State;
	short EffectID;
	float Gain;
	HCHANNEL Channel;
	Vector3 Origin;
};

struct SoundTrackSlot
{
	HSTREAM Channel;
	std::string Track;
};

struct SampleInfo
{
	short Number;
	byte Volume;
	byte Radius;
	byte Randomness;
	signed char Pitch;
	short Flags;
};

struct SoundTrackInfo
{
	std::string Name;
	SOUNDTRACK_PLAYTYPE Mode;
	short Mask;
};

extern std::map<std::string, int> SoundTrackMap;
extern std::vector<SoundTrackInfo> SoundTracks;

extern std::string CurrentLoopedSoundTrack;

long SoundEffect(int effectID, PHD_3DPOS* position, int env_flags, float pitchMultiplier = 1.0f, float gainMultiplier = 1.0f);
void StopSoundEffect(short effectID);
bool LoadSample(char *buffer, int compSize, int uncompSize, int currentIndex);
void FreeSamples();
void StopAllSounds();

void PlaySoundTrack(std::string trackName, SOUNDTRACK_PLAYTYPE mode);
void PlaySoundTrack(std::string trackName, short mask = 0);
void PlaySoundTrack(int index, short mask = 0);
void StopSoundTracks();
void ClearSoundTrackMasks();
void PlaySecretTrack();
void SayNo();
void PlaySoundSources();
int  GetShatterSound(int shatterID);

static void CALLBACK Sound_FinishOneshotTrack(HSYNC handle, DWORD channel, DWORD data, void* userData);

void  SetVolumeMusic(int vol);
void  SetVolumeFX(int vol);

void  Sound_Init();
void  Sound_DeInit();
bool  Sound_CheckBASSError(const char* message, bool verbose, ...);
void  Sound_UpdateScene();
void  Sound_FreeSample(int index);
int   Sound_GetFreeSlot();
void  Sound_FreeSlot(int index, unsigned int fadeout = 0);
int   Sound_EffectIsPlaying(int effectID, PHD_3DPOS *position);
float Sound_DistanceToListener(PHD_3DPOS *position);
float Sound_DistanceToListener(Vector3 position);
float Sound_Attenuate(float gain, float distance, float radius);
bool  Sound_UpdateEffectPosition(int index, PHD_3DPOS *position, bool force = false);
bool  Sound_UpdateEffectAttributes(int index, float pitch, float gain);
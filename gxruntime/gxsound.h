#ifndef GXSOUND_H
#define GXSOUND_H

#include "gxchannel.h"
#include "asyncsound.h"

class gxAudio;
struct FSOUND_SAMPLE;

class gxSound {
public:
	gxAudio* audio;

	gxSound(gxAudio* audio, FSOUND_SAMPLE* sample);
	gxSound(gxAudio* audio, const std::shared_ptr<AsyncSoundLoader::Job>& job, bool use_3d);
	~gxSound();

	static void flushAll();

private:
	bool defs_valid;
	int def_freq, def_vol, def_pan, def_pri;
	FSOUND_SAMPLE* sample;
	std::shared_ptr<AsyncSoundLoader::Job> job;
	std::shared_ptr<std::vector<char>> sampleData;
	bool use_3d;
	bool materialized;
	bool failed;
	float pos[3], vel[3];

	void setDefaults();
	void cancelJob();
	bool materialize(bool blocking);

	static std::vector<gxSound*> pending;

	/***** GX INTERFACE *****/
public:
	//actions
	gxChannel* play();
	gxChannel* play3d(const float pos[3], const float vel[3]);

	//modifiers
	void setLoop(bool loop);
	void setPitch(int hertz);
	void setVolume(float volume);
	void setPan(float pan);
};

#endif
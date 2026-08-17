#include "std.h"
#include "gxsound.h"
#include "gxaudio.h"

std::vector<gxSound*> gxSound::pending;

gxSound::gxSound(gxAudio* a, FSOUND_SAMPLE* s) :
	audio(a), sample(s), job(), use_3d(false), materialized(true), failed(false), defs_valid(true) {
	FSOUND_Sample_GetDefaults(sample, &def_freq, &def_vol, &def_pan, &def_pri);
}

gxSound::gxSound(gxAudio* a, const std::shared_ptr<AsyncSoundLoader::Job>& j, bool u3d) :
	audio(a), sample(0), job(j), use_3d(u3d), materialized(false), failed(false), defs_valid(false) {
	pending.push_back(this);
}

gxSound::~gxSound() {
	cancelJob();
	if (sample) FSOUND_Sample_Free(sample);
}

void gxSound::cancelJob() {
	for (auto it = pending.begin(); it != pending.end(); ++it) {
		if (*it == this) {
			pending.erase(it);
			break;
		}
	}
	if (job) {
		AsyncSoundLoader::instance().cancel(job);
		job.reset();
	}
}

bool gxSound::materialize(bool blocking) {
	if (materialized) return !failed;

	if (job) {
		if (blocking) {
			AsyncSoundLoader::instance().wait(job);
		}
		else {
			int s = job->state.load();
			if (s == AsyncSoundLoader::STATE_QUEUED || s == AsyncSoundLoader::STATE_DECODING) return false;
		}
		if (job->state.load() == AsyncSoundLoader::STATE_FAILED) {
			failed = true;
			cancelJob();
			materialized = true;
			return false;
		}
		if (job->state.load() == AsyncSoundLoader::STATE_CANCELLED) {
			failed = true;
			cancelJob();
			materialized = true;
			return false;
		}

		std::shared_ptr<std::vector<char>> data = job->data;
		if (!data || data->empty()) {
			failed = true;
			cancelJob();
			materialized = true;
			return false;
		}

		int flags = FSOUND_NORMAL | FSOUND_LOADMEMORY | (use_3d ? FSOUND_FORCEMONO : FSOUND_2D);
		sample = FSOUND_Sample_Load(FSOUND_FREE, data->data(), flags, 0, (int)data->size());
		cancelJob();
		if (!sample) {
			failed = true;
			materialized = true;
			return false;
		}
		FSOUND_Sample_GetDefaults(sample, &def_freq, &def_vol, &def_pan, &def_pri);
		defs_valid = true;
		materialized = true;
	}
	return !failed;
}

void gxSound::setDefaults() {
	if(!defs_valid) {
		FSOUND_Sample_SetDefaults(sample, def_freq, def_vol, def_pan, def_pri);
		defs_valid = true;
	}
}

gxChannel* gxSound::play() {
	if (!materialize(true)) return 0;
	setDefaults();
	return audio->play(sample);
}

gxChannel* gxSound::play3d(const float pos[3], const float vel[3]) {
	if (!materialize(true)) return 0;
	setDefaults();
	return audio->play3d(sample, pos, vel);
}

void gxSound::setLoop(bool loop) {
	if (!materialize(true)) return;
	FSOUND_Sample_SetMode(sample, loop ? FSOUND_LOOP_NORMAL : FSOUND_LOOP_OFF);
}

void gxSound::setPitch(int hertz) {
	if (!materialize(true)) return;
	def_freq = hertz;
	defs_valid = false;
}

void gxSound::setVolume(float volume) {
	if (!materialize(true)) return;
	def_vol = volume * 255.0f;
	defs_valid = false;
}

void gxSound::setPan(float pan) {
	if (!materialize(true)) return;
	def_pan = (pan + 1.0f) * 127.5f;
	defs_valid = false;
}

void gxSound::flushAll() {
	for (size_t idx = 0; idx < pending.size(); ) {
		gxSound* s = pending[idx];
		s->materialize(false);
		if (s->materialized) {
		}
		else {
			++idx;
		}
	}
}
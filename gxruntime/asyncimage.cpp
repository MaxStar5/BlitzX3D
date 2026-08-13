#include "std.h"
#include "asyncimage.h"

#include <freeimage.h>
#include <chrono>

std::mutex g_freeimage_mutex;

AsyncImageLoader& AsyncImageLoader::instance() {
	static AsyncImageLoader loader;
	return loader;
}

static FIBITMAP* decodeTo32(const std::string& file, int* w, int* h) {
	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(file.c_str(), 0);
	if (fif == FIF_UNKNOWN) fif = FreeImage_GetFIFFromFilename(file.c_str());
	if (fif == FIF_UNKNOWN) return nullptr;

	FIBITMAP* fib = FreeImage_Load(fif, file.c_str(), 0);
	if (!fib) return nullptr;

	FIBITMAP* fib32;
	if (FreeImage_GetBPP(fib) == 32) {
		fib32 = fib;
	}
	else {
		fib32 = FreeImage_ConvertTo32Bits(fib);
		FreeImage_Unload(fib);
	}
	if (!fib32) return nullptr;

	*w = FreeImage_GetWidth(fib32);
	*h = FreeImage_GetHeight(fib32);
	return fib32;
}

AsyncImageLoader::AsyncImageLoader() :
	inFlight(0), shutdown(false) {
	unsigned count = std::thread::hardware_concurrency() / 2;
	if (count < 2) count = 2;
	if (count > 4) count = 4;
	for (unsigned n = 0; n < count; ++n) threads.push_back(std::thread(&AsyncImageLoader::worker, this));
}

AsyncImageLoader::~AsyncImageLoader() {
	{
		std::unique_lock<std::mutex> lock(mutex);
		shutdown = true;
	}
	cv.notify_all();
	for (auto& t : threads) t.join();
}

std::shared_ptr<AsyncImageLoader::Job> AsyncImageLoader::load(const std::string& file) {
	auto job = std::make_shared<Job>(file);
	{
		std::unique_lock<std::mutex> lock(mutex);
		queue.push_back(job);
	}
	cv.notify_one();
	return job;
}

void AsyncImageLoader::worker() {
	for (;;) {
		std::shared_ptr<Job> job;
		{
			std::unique_lock<std::mutex> lock(mutex);
			cv.wait(lock, [this]() { return shutdown || !queue.empty(); });
			if (shutdown && queue.empty()) return;
			job = queue.back();
			queue.pop_back();
			++inFlight;
		}

		if (job->state.load() == STATE_CANCELLED) {
			std::unique_lock<std::mutex> lock(mutex);
			--inFlight;
			cv.notify_all();
			continue;
		}

		job->state.store(STATE_DECODING);

		int w = 0, h = 0;
		FIBITMAP* fib;
		{
			std::unique_lock<std::mutex> lock(g_freeimage_mutex);
			fib = decodeTo32(job->file, &w, &h);
		}

		std::unique_lock<std::mutex> lock(mutex);
		if (job->state.load() == STATE_CANCELLED) {
			--inFlight;
			if (fib) FreeImage_Unload(fib);
			cv.notify_all();
			continue;
		}
		if (fib) {
			if (job->fib32) FreeImage_Unload((FIBITMAP*)job->fib32);
			job->fib32 = fib;
			job->w = w;
			job->h = h;
			job->state.store(STATE_DONE);
		}
		else {
			job->state.store(STATE_FAILED);
		}
		--inFlight;
		cv.notify_all();
	}
}

void AsyncImageLoader::wait(const std::shared_ptr<Job>& job) {
	std::unique_lock<std::mutex> lock(mutex);
	if (cv.wait_for(lock, std::chrono::seconds(10), [&]() {
		int s = job->state.load();
		return s == STATE_DONE || s == STATE_FAILED || s == STATE_CANCELLED;
	})) return;

	//decode synchronously as fallback to avoid infinite loads
	for (auto it = queue.begin(); it != queue.end(); ++it) {
		if (*it == job) { queue.erase(it); break; }
	}
	job->state.store(STATE_CANCELLED);
	lock.unlock();

	int w = 0, h = 0;
	FIBITMAP* fib;
	{
		std::unique_lock<std::mutex> fim(g_freeimage_mutex);
		fib = decodeTo32(job->file, &w, &h);
	}

	lock.lock();
	if (fib) {
		if (job->fib32) FreeImage_Unload((FIBITMAP*)job->fib32);
		job->fib32 = fib;
		job->w = w;
		job->h = h;
		job->state.store(STATE_DONE);
	}
	else {
		job->state.store(STATE_FAILED);
	}
}

void AsyncImageLoader::waitAll() {
	std::unique_lock<std::mutex> lock(mutex);
	cv.wait_for(lock, std::chrono::seconds(20), [this]() { return queue.empty() && inFlight == 0; });
}

void AsyncImageLoader::cancel(const std::shared_ptr<Job>& job) {
	std::unique_lock<std::mutex> lock(mutex);
	for (auto it = queue.begin(); it != queue.end(); ++it) {
		if (*it == job) {
			queue.erase(it);
			break;
		}
	}
	int s = job->state.load();
	if (s == STATE_DONE || s == STATE_FAILED) {
		if (job->fib32) {
			std::unique_lock<std::mutex> fim(g_freeimage_mutex);
			FreeImage_Unload((FIBITMAP*)job->fib32);
			job->fib32 = nullptr;
		}
	}
	job->state.store(STATE_CANCELLED);
}
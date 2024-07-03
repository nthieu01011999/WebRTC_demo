#include <unistd.h>
#include <ctime>
#include <arpa/inet.h>
#include <sys/time.h>
#include <chrono>
// #include <optional>

#include "stream.hpp"
#include "app_data.h"
#include "h26xsource.hpp"

/* =======================================================================================
 *	S T R E A M			C O N S T R U C T O R
 * =======================================================================================
 * Methods:
 * 		Stream
 * 		~Stream
 *-----------------------------------------------------------------------------------------
 */

std::vector<std::byte> Stream::nalUnitsMain;
std::vector<std::byte> Stream::nalUnitsSub;

Stream::Stream(std::shared_ptr<MediaStream> mediaLive, std::shared_ptr<MediaStream> mediaPLayback)
	: std::enable_shared_from_this<Stream>(), mediaLive(mediaLive), mediaPLayback(mediaPLayback) {
	mPOSIXMutex = PTHREAD_MUTEX_INITIALIZER;
	mPOSIXCond	= PTHREAD_COND_INITIALIZER;
}

Stream::~Stream() {
	pthread_mutex_destroy(&mPOSIXMutex);
	pthread_cond_destroy(&mPOSIXCond);

	std::cout << "[DECONSTRUCTOR] Media stream has coslaped" << std::endl;
}

/* =======================================================================================
 *	S T R E A M			M E T H O D S
 * =======================================================================================
 * Properties:
 * 		mPOSIXMutex
 * 		mPOSIXCond
 * 		mPbSampleThreadId
 * 		mIsStarted
 * 		mIsPbRunning
 * 		mPbSampleHdl
 * Methods:
 * 		unsafePrepareForSample
 * 		sendPbSample
 * 		start
 *		stop
 *		triggerPbSession
 *		pendingPbSession
 * 		onPbSampleHdl
 *-----------------------------------------------------------------------------------------
 */
std::pair<std::shared_ptr<StreamSource>, StreamSourceType> Stream::unsafePrepareForSample() {
	std::shared_ptr<StreamSource> ss;
	StreamSourceType sst;
	uint64_t nextTime;

	if (mediaPLayback->getSampleTime_us(StreamSourceType::Audio) < mediaPLayback->getSampleTime_us(StreamSourceType::Video)) {
		ss	= mediaPLayback->audio;
		sst = StreamSourceType::Audio;
	}
	else {
		ss	= mediaPLayback->video;
		sst = StreamSourceType::Video;
	}
	nextTime = mediaPLayback->getSampleTime_us(sst);

	auto currentTime = currentTimeInMicroSeconds();
	auto elapsed	 = currentTime - mediaPLayback->startTime;

	if (nextTime > elapsed) {
		auto waitTime = nextTime - elapsed;
		pthread_mutex_unlock(&mPOSIXMutex);
		usleep(waitTime);
		pthread_mutex_lock(&mPOSIXMutex);
	}

	return {ss, sst};
}

void *Stream::sendPbSample(void *arg) {
	Stream *argStream = static_cast<Stream *>(arg);

	while (argStream->mIsStarted) {
		pthread_mutex_lock(&argStream->mPOSIXMutex);

		if (argStream->mIsPbRunning == false) {
			pthread_cond_wait(&argStream->mPOSIXCond, &argStream->mPOSIXMutex);
		}

		auto ssSST = argStream->unsafePrepareForSample();
		auto ss	   = ssSST.first;
		auto sst   = ssSST.second;

		argStream->mPbSampleHdl(sst, ss->getSampleTime_us());
		ss->loadNextTime();

		pthread_mutex_unlock(&argStream->mPOSIXMutex);
	}

	return NULL;
}

void Stream::start() {
	pthread_mutex_lock(&mPOSIXMutex);

	if (mIsStarted) {
		pthread_mutex_unlock(&mPOSIXMutex);
		return;
	}

	mediaLive->start();
	mIsStarted = true;

	pthread_mutex_unlock(&mPOSIXMutex);

	pthread_create(&mPbSampleThreadId, NULL, sendPbSample, this);
	pthread_setname_np(mPbSampleThreadId, "sendpb task");
}

void Stream::stop() {
	pthread_mutex_lock(&mPOSIXMutex);

	if (!mIsStarted) {
		pthread_mutex_unlock(&mPOSIXMutex);
		return;
	}

	mediaLive->stop();
	mediaPLayback->stop();
	mIsStarted	 = false;
	mIsPbRunning = false;

	pthread_mutex_unlock(&mPOSIXMutex);

	pthread_cond_signal(&mPOSIXCond);	   /* Condition trigger to exit */
	pthread_join(mPbSampleThreadId, NULL); /* Waiting for thread completely escaped */
};

void Stream::triggerPbSession() {
	pthread_mutex_lock(&mPOSIXMutex);

	if (mIsPbRunning == false) {
		mIsPbRunning = true;
		mediaPLayback->start();
	}

	pthread_mutex_unlock(&mPOSIXMutex);

	pthread_cond_signal(&mPOSIXCond);
}

void Stream::pendingPbSession() {
	mIsPbRunning = false;
	mediaPLayback->stop();
}

void Stream::onPbSampleHdl(std::function<void(StreamSourceType, uint64_t)> handler) {
	mPbSampleHdl = handler;
}

/* =======================================================================================
 *	S T A T I C		S T R E A M			I M P L E M E N T A T I O N
 * =======================================================================================
 * Properties:
 * 		pubLicStreamPOSIXMutex
 * Methods:
 * 		pubLicStreamPOSIXMutexLOCK
 * 		pubLicStreamPOSIXMutexUNLOCK
 * 		syncLiveWaitTime
 *		MediaLiveVideo
 *		MediaLiveAudio
 *-----------------------------------------------------------------------------------------
 */
pthread_mutex_t Stream::pubLicStreamPOSIXMutex = PTHREAD_MUTEX_INITIALIZER;

void Stream::pubLicStreamPOSIXMutexLOCK() {
    pthread_mutex_lock(&pubLicStreamPOSIXMutex);
}


void Stream::pubLicStreamPOSIXMutexUNLOCK() {
	pthread_mutex_unlock(&pubLicStreamPOSIXMutex);
}

static void syncLiveWaitTime(std::shared_ptr<Stream> ref) {
	uint64_t nextTime;

	if (ref->mediaLive->getSampleTime_us(StreamSourceType::Video) > ref->mediaLive->getSampleTime_us(StreamSourceType::Audio)) {
		nextTime = ref->mediaLive->getSampleTime_us(StreamSourceType::Audio);
	}
	else {
		nextTime = ref->mediaLive->getSampleTime_us(StreamSourceType::Video);
	}

	auto currentTime = currentTimeInMicroSeconds();
	auto elapsed	 = currentTime - ref->mediaLive->startTime;

	if (nextTime > elapsed) {
		auto waitTime = nextTime - elapsed;

		Stream::pubLicStreamPOSIXMutexUNLOCK();
		usleep(waitTime);
		Stream::pubLicStreamPOSIXMutexLOCK();
	}
}

void Stream::MediaLiveVideo(bool isFullHD, uint8_t *samples, uint32_t totalSamples) {
	
	extern ClientsGroup_t clients;
	extern optional<shared_ptr<Stream>> avStream;

	// APP_DBG("[MediaLiveVideo]: Enter function\n");

	if (!avStream.has_value() || clients.empty()) {
		// //APP_DBG("MediaLiveVideo: NO active stream or clients available\n");
		return;
	}

	auto avStreamValue = avStream.value();
	rtc::binary *pSample = nullptr;

	Stream::pubLicStreamPOSIXMutexLOCK();
	{
		//APP_DBG("MediaLiveVideo: Acquired stream LOCK\n");
		syncLiveWaitTime(avStreamValue);
		avStreamValue->mediaLive->video->loadNextTime();
		avStreamValue->mediaLive->loadNextSample(samples, totalSamples, StreamSourceType::Video);
		pSample = &(avStreamValue->mediaLive->getSample(StreamSourceType::Video));
	}
	Stream::pubLicStreamPOSIXMutexUNLOCK();

	if (!pSample) {
		//APP_DBG("MediaLiveVideo: No sample available\n");
		return;
	}

	lockMutexListClients();
	{
		//APP_DBG("MediaLiveVideo: Locked clients mutex\n");
		for (auto it : clients) {
			//APP_DBG("MediaLiveVideo: Processing client %s \n", it.first.c_str());

			Client::lockSignalingClient();
			if (Client::getSignalingStatus()) {
				//APP_DBG("MediaLiveVideo: Signaling is active, ignoring buffer video for client %s\n", it.first.c_str());
				Client::unLockSignalingClient();
				break;
			}
			Client::unLockSignalingClient();

			auto id = it.first;
			auto client = it.second;
			auto optTrackData = client->video;

			if (client->getState() == Client::State::Ready && optTrackData.has_value()) {
				if (client->getMediaStreamOptions() == Client::eOptions::LiveStream) {
					bool isValid = false;
					if (isFullHD) {
						isValid = (client->getLiveResolution() == LiveStream::eResolution::FullHD1080p);
					} else {
						isValid = (client->getLiveResolution() == LiveStream::eResolution::HD720p);
					}
					if (!isValid) {
						//APP_DBG("MediaLiveVideo: Resolution mismatch for client %s\n", id.c_str());
						continue;
					}

					auto trackData = optTrackData.value();
					auto rtpConfig = trackData->sender->rtpConfig;

					auto elapsedSeconds = double(avStreamValue->mediaLive->getSampleTime_us(StreamSourceType::Video)) / (1000 * 1000);
					uint32_t elapsedTimestamp = rtpConfig->secondsToTimestamp(elapsedSeconds);
					rtpConfig->timestamp = rtpConfig->startTimestamp + elapsedTimestamp;
					auto reportElapsedTimestamp = rtpConfig->timestamp - trackData->sender->lastReportedTimestamp();
					if (rtpConfig->timestampToSeconds(reportElapsedTimestamp) > 1) {
						trackData->sender->setNeedsToReport();
					}

					try {
						trackData->track->send(*pSample);
						//APP_DBG("MediaLiveVideo: Sent video packet to client %s\n", id.c_str());
					} catch (const std::exception &e) {
						APP_DBG("[ERR] Unable to send video packet to client %s, err: %s\n", id.c_str(), e.what());
					}
				} else {
					//APP_DBG("MediaLiveVideo: Client %s is not in LiveStream mode\n", id.c_str());
				}
			} else {
				//APP_DBG("MediaLiveVideo: Client %s is not ready or has no video track\n", id.c_str());
			}
		}
	}
	unlockMutexListClients();

// #if 0
#if 1
	pSample->clear();
#else
	avStreamValue->mediaLive->rstSample(StreamSourceType::Video);
#endif

	//APP_DBG("MediaLiveVideo: Exit function\n");
}



void Stream::MediaLiveAudio(uint8_t *samples, uint32_t totalSamples) {

}
/**
 * libdatachannel streamer example
 * Copyright (c) 2020 Filip Klembara (in2core)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "helpers.hpp"
#include "utils.h"

#include <ctime>
#include <arpa/inet.h>
#include <sys/time.h>
#include <chrono>

#include "app.h"
#include "app_data.h"
#include "task_list.h"

#include <chrono>

#include "app.h"
#include "app_data.h"
#include "task_list.h"

using namespace rtc;
using namespace std::chrono;

pthread_mutex_t Client::mtxSignaling				= PTHREAD_MUTEX_INITIALIZER;
bool Client::isSignalingRunning						= false;
std::atomic<int> Client::totalClientsConnectSuccess = 0;
std::string Client::currentClientPushToTalk;

Client::Client(std::shared_ptr<rtc::PeerConnection> pc) {
	_peerConnection = pc;
	mPOSIXMutex		= PTHREAD_MUTEX_INITIALIZER;
	mDownloadMutex	= PTHREAD_MUTEX_INITIALIZER;
	mIsSignalingOk	= false;
}

Client::~Client() {
	APP_DBG("~Client id: %s\n", mId.c_str());
	pthread_mutex_destroy(&mPOSIXMutex);
	pthread_mutex_destroy(&mDownloadMutex);
	if (mIsSignalingOk) {
		if (--Client::totalClientsConnectSuccess < 0) {
			APP_DBG("Client::totalClientsConnectSuccess < 0\n");
			FATAL("RTC", 0x02);
		}
		APP_DBG("total client: %d\n", Client::totalClientsConnectSuccess.load());
	}
}

void Client::setState(State mState) {
	this->mState = mState;
}

Client::State Client::getState() {
	return mState;
}

// bool Client::getDownloadFlag() {
// 	bool ret;
// 	pthread_mutex_lock(&mDownloadMutex);
// 	ret = mDownloadFlag;
// 	pthread_mutex_unlock(&mDownloadMutex);
// 	return ret;
// }

// void Client::setDownloadFlag(bool newDownloadFlag) {
// 	pthread_mutex_lock(&mDownloadMutex);
// 	mDownloadFlag = newDownloadFlag;
// 	pthread_mutex_unlock(&mDownloadMutex);
// }

// fileDownloadInfo_t Client::getCurrentFileTransfer() {
// 	return mCurrentFileTransfer;
// }

// void Client::setCurrentFileTransfer(const fileDownloadInfo_t &newCurrentFileTransfer) {
// 	mCurrentFileTransfer = newCurrentFileTransfer;
// }

// uint32_t Client::getCursorFile() {
// 	return mCursorFile;
// }

// void Client::setCursorFile(uint32_t newCursorFile) {
// 	mCursorFile = newCursorFile;
// }

// void Client::startTimeoutDownload() {
// 	string id = getId();
// 	setTimerDownloadId(systemTimer.add(milliseconds(GW_WEBRTC_WAIT_REQUEST_TIMEOUT_INTERVAL), [id](CppTime::timer_id) {
// 		task_post_dynamic_msg(GW_TASK_WEBRTC_ID, GW_WEBRTC_DATACHANNEL_DOWNLOAD_RELEASE_REQ, (uint8_t *)id.c_str(), id.length() + 1);
// 	}));
// }

// void Client::removeTimeoutDownload() {
// 	int idT = getTimerDownloadId();
// 	if (idT >= 0) {
// 		systemTimer.remove(idT);
// 		setTimerDownloadId(-1);
// 	}
// }

// void Client::startTimeoutConnect() {
// 	string id = getId();
// 	setTimerConnectId(systemTimer.add(milliseconds(GW_WEBRTC_ERASE_CLIENT_NO_ANSWER_TIMEOUT_INTERVAL), [id](CppTime::timer_id) {
// 		task_post_dynamic_msg(GW_TASK_WEBRTC_ID, GW_WEBRTC_CHECK_CLIENT_CONNECTED_REQ, (uint8_t *)id.c_str(), id.length() + 1);
// 	}));
// }

void Client::removeTimeoutConnect() {
	int idT = getTimerConnectId();
	if (idT >= 0) {
		systemTimer.remove(idT);
		setTimerConnectId(-1);
	}
}

int Client::getTimerConnectId() {
	return mTimerConnectId;
}

void Client::setTimerConnectId(size_t newTimerConnectId) {
	mTimerConnectId = newTimerConnectId;
}

// int Client::getTimerDownloadId() {
// 	return mTimerDownloadId;
// }

// void Client::setTimerDownloadId(size_t newTimerDownloadId) {
// 	mTimerDownloadId = newTimerDownloadId;
// }

/* =======================================================================================
 *	M E D I A		S T R E A M		M E T H O D S		I M P L E M E N T A T I O N
 * =======================================================================================
 * Public:
 * 		setMediaStreamOptions
 * 		setLiveResolution
 * 		getMediaStreamOptions
 * 		getLiveResolution
 * 		assignRecord
 * 		setPbControl
 * 		getPbStatus
 * 		getPbTimeSpentInSecs
 * 		getVideoPbAttributes
 * 		getAudioPbAttributes
 * Private:
 *		openPbSession
 *		closePbSession
 *		seekPosRecordInSecs
 *		setPbSpeed
 *-----------------------------------------------------------------------------------------
 */
void Client::setMediaStreamOptions(eOptions opt) {
	pthread_mutex_lock(&mPOSIXMutex);
	mOptions = opt;
	pthread_mutex_unlock(&mPOSIXMutex);
}

Client::eOptions Client::getMediaStreamOptions() {
	pthread_mutex_lock(&mPOSIXMutex);
	auto ret = mOptions;
	pthread_mutex_unlock(&mPOSIXMutex);

	return ret;
}

void Client::setLiveResolution(LiveStream::eResolution res) {
	pthread_mutex_lock(&mPOSIXMutex);
	mLiveResolution = res;
	pthread_mutex_unlock(&mPOSIXMutex);
}

LiveStream::eResolution Client::getLiveResolution() {
	pthread_mutex_lock(&mPOSIXMutex);
	auto ret = mLiveResolution;
	pthread_mutex_unlock(&mPOSIXMutex);

	return ret;
}

// void Client::assignRecord(std::string pathToRecords, std::string dateTime, std::string desc) {
// 	pthread_mutex_lock(&mPOSIXMutex);

// 	mVideoPbAttributes.url = pathToRecords + std::string("/video/") + dateTime + std::string("/") + desc + std::string(".h264");
// 	mAudioPbAttributes.url = pathToRecords + std::string("/audio/") + dateTime + std::string("/") + desc + std::string(".g711");

// 	auto stSplit					 = string_split(desc, "_");
// 	mVideoPbAttributes.durationInSec = mAudioPbAttributes.durationInSec = std::stoi(stSplit[2]) - std::stoi(stSplit[1]);

// 	pthread_mutex_unlock(&mPOSIXMutex);
// }

// void Client::setPbControl(PlayBack::ePbControl pbc, uint32_t *argv) {
// 	pthread_mutex_lock(&mPOSIXMutex);

// 	switch (pbc) {
// 	case PlayBack::ePbControl::Play: {
// 		mPbStatus = PlayBack::ePbStatus::Playing;
// 		openPbSession();
// 	} break;

// 	case PlayBack::ePbControl::Stop: {
// 		closePbSession();
// 		mPbStatus = PlayBack::ePbStatus::Stopped;
// 	} break;

// 	case PlayBack::ePbControl::Resume: {
// 		mPbStatus = PlayBack::ePbStatus::Playing;
// 	} break;

// 	case PlayBack::ePbControl::Pause: {
// 		mPbStatus = PlayBack::ePbStatus::Pausing;
// 	} break;

// 	case PlayBack::ePbControl::Seek: {
// 		seekPosRecordInSecs(*argv);
// 		// mPbStatus = PlayBack::ePbStatus::Playing;
// 	} break;

// 	case PlayBack::ePbControl::Speed: {
// 		// mPbStatus = PlayBack::ePbStatus::Playing;
// 		setPbSpeed((PlayBack::ePbSpeedLoad)*argv);
// 	} break;

// 	default:
// 		break;
// 	}

// 	pthread_mutex_unlock(&mPOSIXMutex);
// }

// void Client::openPbSession() {
// 	mVideoPbAttributes.fileSize = mVideoPbAttributes.track = 0;
// 	mAudioPbAttributes.fileSize = mAudioPbAttributes.track = 0;
// 	mVideoPbAttributes.loadSpeed = mAudioPbAttributes.loadSpeed = PlayBack::ePbSpeedLoad::Normal;

// 	if (!mVideoPbAttributes.url.empty()) {
// 		mVideoPbAttributes.fileSize = sizeOfFile(mVideoPbAttributes.url.c_str());
// 	}

// 	if (!mAudioPbAttributes.url.empty()) {
// 		mAudioPbAttributes.fileSize = sizeOfFile(mAudioPbAttributes.url.c_str());
// 	}

// 	if (mVideoPbAttributes.fileSize > 0 && mAudioPbAttributes.fileSize > 0) {
// 		mPbStatus							 = PlayBack::ePbStatus::Playing;
// 		mVideoPbAttributes.lastSecondsTick	 = 0;
// 		mVideoPbAttributes.sampleDuration_us = 1000 * 1000 / PLAYBACK_VIDEO_FPS;
// 		mVideoPbAttributes.sampleTime_us	 = std::numeric_limits<uint64_t>::max() - mVideoPbAttributes.sampleDuration_us + 1;
// 		mVideoPbAttributes.nbrNalusLoad		 = 1;
// 		mAudioPbAttributes.sampleDuration_us = 1000 * 1000 / PLAYBACK_AUDIO_SPS;
// 		mAudioPbAttributes.sampleTime_us	 = std::numeric_limits<uint64_t>::max() - mAudioPbAttributes.sampleDuration_us + 1;
// 		mAudioPbAttributes.nbrNalusLoad		 = 1;
// 	}
// 	else {
// 		mPbStatus = PlayBack::ePbStatus::Error;
// 	}
// }

// void Client::closePbSession() {
// 	mVideoPbAttributes.url.clear();
// 	mVideoPbAttributes.track = 0;
// 	mAudioPbAttributes.url.clear();
// 	mAudioPbAttributes.track = 0;
// }

// void Client::setPbSpeed(PlayBack::ePbSpeedLoad speed) {
// 	int8_t n = 1;

// 	switch (speed) {
// 	case PlayBack::ePbSpeedLoad::Slow:
// 		n = -1;
// 		break;
// 	case PlayBack::ePbSpeedLoad::Normal:
// 		n = 1;
// 		break;
// 	case PlayBack::ePbSpeedLoad::FastX2:
// 		n = 2;
// 		break;
// 	case PlayBack::ePbSpeedLoad::FastX4:
// 		n = 4;
// 		break;
// 	case PlayBack::ePbSpeedLoad::FastX8:
// 		n = 8;
// 		break;
// 	case PlayBack::ePbSpeedLoad::FastX16:
// 		n = 16;
// 		break;
// 	default:
// 		break;
// 	}

// 	mVideoPbAttributes.loadSpeed = mAudioPbAttributes.loadSpeed = speed;
// 	mVideoPbAttributes.nbrNalusLoad = mAudioPbAttributes.nbrNalusLoad = n;
// }

PlayBack::ePbStatus Client::getPbStatus() {
	pthread_mutex_lock(&mPOSIXMutex);

	auto ret = mPbStatus;
	if (mVideoPbAttributes.track >= mVideoPbAttributes.fileSize) {
		ret = PlayBack::ePbStatus::Done;
	}

	pthread_mutex_unlock(&mPOSIXMutex);

	return ret;
}

// void Client::seekPosRecordInSecs(uint32_t inSecs) {
// 	if (inSecs < mVideoPbAttributes.durationInSec) {
// 		mVideoPbAttributes.track = (inSecs / 2) * (mVideoPbAttributes.fileSize / (mVideoPbAttributes.durationInSec / 2));
// 		mAudioPbAttributes.track = (inSecs / 2) * (mAudioPbAttributes.fileSize / (mAudioPbAttributes.durationInSec / 2));
// 	}
// }

uint32_t Client::getPbTimeSpentInSecs() {
	pthread_mutex_lock(&mPOSIXMutex);
	auto ret = (uint32_t)(((double)mVideoPbAttributes.track / mVideoPbAttributes.fileSize) * mVideoPbAttributes.durationInSec);
	pthread_mutex_unlock(&mPOSIXMutex);

	return ret;
}

SDSource *Client::getVideoPbAttributes() {
	pthread_mutex_lock(&mPOSIXMutex);
	auto ret = (&mVideoPbAttributes);
	pthread_mutex_unlock(&mPOSIXMutex);

	return ret;
}

SDSource *Client::getAudioPbAttributes() {
	pthread_mutex_lock(&mPOSIXMutex);
	auto ret = (&mAudioPbAttributes);
	pthread_mutex_unlock(&mPOSIXMutex);

	return ret;
}

string Client::getId() {
	return mId;
}

void Client::setId(const string &newId) {
	mId = newId;
}

void Client::setSignalingStatus(bool value) {
	pthread_mutex_lock(&mtxSignaling);
	isSignalingRunning = value;
	pthread_mutex_unlock(&mtxSignaling);
}

bool Client::getSignalingStatus() {
	return isSignalingRunning;
}

void Client::lockSignalingClient() {
	pthread_mutex_lock(&mtxSignaling);
}

void Client::unLockSignalingClient() {
	pthread_mutex_unlock(&mtxSignaling);
}

void Client::startTimeoutConnect() {
	string id = getId();
	setTimerConnectId(systemTimer.add(milliseconds(GW_WEBRTC_ERASE_CLIENT_NO_ANSWER_TIMEOUT_INTERVAL), [id](CppTime::timer_id) {
		task_post_dynamic_msg(GW_TASK_WEBRTC_ID, GW_WEBRTC_CHECK_CLIENT_CONNECTED_REQ, (uint8_t *)id.c_str(), id.length() + 1);
	}));
}

bool Client::isAlive() {
    // Check if the peer connection is still active
    if (_peerConnection) {
        auto state = _peerConnection->state();
        if (state == rtc::PeerConnection::State::Connected || 
            state == rtc::PeerConnection::State::Connecting) {
            return true;
        }
    }
    return false;
}
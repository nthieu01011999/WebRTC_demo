/**
 * libdatachannel streamer example
 * Copyright (c) 2020 Filip Klembara (in2core)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef helpers_hpp
#define helpers_hpp

#include <vector>
#include <shared_mutex>
#include <string>

#include "app_dbg.h"
#include "rtc/rtc.hpp"
#include "stream.hpp"
#include "streamsource.hpp"

using namespace std;

typedef struct t_fileDownloadInfo {
	string type;
	string path;
	uint32_t size;
} fileDownloadInfo_t;

struct ClientTrackData {
	std::shared_ptr<rtc::Track> track;
	std::shared_ptr<rtc::RtcpSrReporter> sender;

	ClientTrackData(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::RtcpSrReporter> sender) {
		this->track	 = track;
		this->sender = sender;
	}
};

struct ClientTrack {
	std::string id;
	std::shared_ptr<ClientTrackData> trackData;

	ClientTrack(std::string id, std::shared_ptr<ClientTrackData> trackData) {
		this->id		= id;
		this->trackData = trackData;
	}
};

class Client {
public:
	enum class State {
		Waiting,
		WaitingForVideo,
		WaitingForAudio,
		Ready
	};

	enum class eOptions {
		Idle,
		LiveStream,
		Playback,
	};

	enum class ePushToTalkREQStatus {
		Begin,
		Stop
	};

	Client(std::shared_ptr<rtc::PeerConnection> pc);
	~Client();
	bool isAlive();
	void setState(State state);
	State getState();

	bool getDownloadFlag();
	void setDownloadFlag(bool newIsDownloading);

	fileDownloadInfo_t getCurrentFileTransfer();
	void setCurrentFileTransfer(const fileDownloadInfo_t &newCurrentFileTransfer);
	uint32_t getCursorFile();
	void setCursorFile(uint32_t newCursorFile);
	void startTimeoutDownload();
	void removeTimeoutDownload();
	void startTimeoutConnect();
	void removeTimeoutConnect();

	/* Media Stream Methods (Live & PLayBack) */
	void setMediaStreamOptions(eOptions opt);
	eOptions getMediaStreamOptions();
	void setLiveResolution(LiveStream::eResolution res);
	LiveStream::eResolution getLiveResolution();
	void assignRecord(std::string pathToRecords, std::string dateTime, std::string desc);
	void setPbControl(PlayBack::ePbControl pbc, uint32_t *argv);
	PlayBack::ePbStatus getPbStatus();
	uint32_t getPbTimeSpentInSecs();
	SDSource *getVideoPbAttributes();
	SDSource *getAudioPbAttributes();

	std::string getId();
	void setId(const std::string &newId);
	static void setSignalingStatus(bool value);
	static bool getSignalingStatus();
	static void lockSignalingClient();
	static void unLockSignalingClient();
	static void cleanup() {
		pthread_mutex_destroy(&mtxSignaling);
	}

public:
	const std::shared_ptr<rtc::PeerConnection> &peerConnection = _peerConnection;
	std::optional<std::shared_ptr<ClientTrackData>> video;
	std::optional<std::shared_ptr<ClientTrackData>> audio;
	std::optional<std::shared_ptr<rtc::DataChannel>> dataChannel;
	std::vector<fileDownloadInfo_t> arrFilesDownload;
	static std::atomic<int> totalClientsConnectSuccess;
	std::atomic<bool> mIsSignalingOk;

	static std::string currentClientPushToTalk;

private:
	void openPbSession();
	void closePbSession();
	void seekPosRecordInSecs(uint32_t inSecs);
	void setPbSpeed(PlayBack::ePbSpeedLoad speed);

	int getTimerConnectId();
	void setTimerConnectId(size_t newTimerConnectId);
	int getTimerDownloadId();
	void setTimerDownloadId(size_t newTimerDownloadId);

	pthread_mutex_t mPOSIXMutex, mDownloadMutex;
	State mState = State::Waiting;
	std::string mId;
	std::shared_ptr<rtc::PeerConnection> _peerConnection;

	bool mDownloadFlag	 = false;
	int mTimerDownloadId = -1, mTimerConnectId = -1;
	fileDownloadInfo_t mCurrentFileTransfer;
	uint32_t mCursorFile;

	eOptions mOptions						= eOptions::Idle;
	LiveStream::eResolution mLiveResolution = LiveStream::eResolution::HD720p;
	PlayBack::ePbStatus mPbStatus			= PlayBack::ePbStatus::Stopped;
	SDSource mVideoPbAttributes;
	SDSource mAudioPbAttributes;

	static pthread_mutex_t mtxSignaling;
	static bool isSignalingRunning;
};

typedef std::unordered_map<std::string, std::shared_ptr<Client>> ClientsGroup_t;

#endif /* helpers_hpp */

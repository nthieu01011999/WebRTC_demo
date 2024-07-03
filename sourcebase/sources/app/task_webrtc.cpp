#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <future>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <chrono>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <atomic>
#include "app.h"
#include "app_data.h"
#include "app_config.h"
#include "app_dbg.h"
#include "datachannel_hdl.h"
#include "task_list.h"
#include "helpers.hpp"
#include "rtc/rtc.hpp"
#include "json.hpp"
#include "stream.hpp"
#ifdef BUILD_ARM_VVTK
#include "h26xsource.hpp"
#include "audiosource.hpp"
#endif
#include "parser_json.h"
#include "utils.h"

#define CLIENT_SIGNALING_MAX 20
#define CLIENT_MAX 10

using namespace rtc;
using namespace std;
using namespace chrono_literals;
using namespace chrono;
using json = nlohmann::json;

// Global variables
q_msg_t gw_task_webrtc_mailbox;
std::shared_ptr<WebSocket> globalWebSocket;
static Configuration rtcConfig;
std::unordered_map<std::string, std::shared_ptr<PeerConnection>> peerConnections;
atomic<bool> isConnected(false);

// Function declarations
shared_ptr<Client> createPeerConnection(const Configuration &rtcConfig, weak_ptr<WebSocket> wws, const string& id);
static int8_t loadWsocketSignalingServerConfigFile(string &wsUrl);
static int8_t loadIceServersConfigFile(Configuration &rtcConfig);
void handleWebSocketMessage(const std::string& message, std::shared_ptr<WebSocket> ws);
void handleClientRequest(const std::string& clientId, std::shared_ptr<WebSocket> ws);
void handleAnswer(const std::string& id, const json& message);
static void addToStream(shared_ptr<Client> client, bool isAddingVideo);
static void startStream();
static shared_ptr<Stream> createStream(void);
static shared_ptr<ClientTrackData> addVideo(const shared_ptr<PeerConnection> pc, const uint8_t payloadType, const uint32_t ssrc, const string cname, const string msid, const function<void(void)> onOpen);
static shared_ptr<ClientTrackData> addAudio(const shared_ptr<PeerConnection> pc, const uint8_t payloadType, const uint32_t ssrc, const string cname, const string msid, const function<void(void)> onOpen, std::string id);
void periodicClientCheck();
void startPeriodicCheck();

void initializeWebSocketServer(const std::string &wsUrl);

//done
void *gw_task_webrtc_entry(void *) {
    ak_msg_t *msg = AK_MSG_NULL;

    wait_all_tasks_started();
    APP_DBG("[STARTED] gw_task_webrtc_entry\n");

    if (loadIceServersConfigFile(rtcConfig) != APP_CONFIG_SUCCESS) {
        APP_PRINT("Failed to load ICE servers configuration.\n");
        return AK_MSG_NULL;  // Exit if configuration fails
    }

    rtcConfig.disableAutoNegotiation = false;  // Set localDescription automatically

    auto ws = make_shared<WebSocket>();
    weak_ptr<WebSocket> wws = ws;
    atomic<bool> isConnected(false);

    initializeWebSocketServer("ws://sig.espitek.com:8089/" + mtce_getSerialInfo());

    std::this_thread::sleep_for(1s);

    if (isConnected.load()) {
        APP_PRINT("WebSocket is successfully connected.\n");
    } else {
        APP_PRINT("WebSocket connection failed or is still in progress.\n");
    }

    startPeriodicCheck();

    while (1) {
        msg = ak_msg_rev(GW_TASK_WEBRTC_ID);

        switch (msg->header->sig) {
        case GW_WEBRTC_ERASE_CLIENT_REQ: {
            APP_DBG_SIG("GW_WEBRTC_ERASE_CLIENT_REQ\n");
            string id((char *)msg->header->payload);
            APP_PRINT("clear client id: %s\n", id.c_str());
            Client::setSignalingStatus(true);
            lockMutexListClients();
            clients.erase(id);
            unlockMutexListClients();
            Client::setSignalingStatus(false);
        } break;

        case GW_WEBRTC_ON_MESSAGE_CONTROL_DATACHANNEL_REQ: {
            APP_DBG_SIG("GW_WEBRTC_ON_MESSAGE_CONTROL_DATACHANNEL_REQ\n");
            try {
                json message = json::parse(string((char *)msg->header->payload, msg->header->len));
                string id = message["ClientId"].get<string>();
                string data = message["Data"].get<string>();
                string resp = "";
                APP_DBG("client id: %s, msg: %s\n", id.c_str(), data.c_str());
                onDataChannelHdl(id, data, resp);
                sendMsgControlDataChannel(id, resp);
            } catch (const exception &error) {
                APP_DBG("%s\n", error.what());
            }
        } break;

        default:
            APP_DBG_SIG("[DEBUG] Unknown message signal received: %d\n", msg->header->sig);
            break;
        }

        ak_msg_free(msg);
    }

    return (void *)0;
}
//done

void addToStream(shared_ptr<Client> client, bool isAddingVideo) {
	if (client->getState() == Client::State::Waiting) {
		client->setState(isAddingVideo ? Client::State::WaitingForAudio : Client::State::WaitingForVideo);
	}
	else if ((client->getState() == Client::State::WaitingForAudio && !isAddingVideo) || (client->getState() == Client::State::WaitingForVideo && isAddingVideo)) {
		// Audio and video tracks are collected now
		assert(client->video.has_value() && client->audio.has_value());
		auto video = client->video.value();

		if (avStream.has_value()) {
			// sendInitialNalus(avStream.value(), video);
		}

		client->setState(Client::State::Ready);
	}
	if (client->getState() == Client::State::Ready) {
		startStream();
	}
}

void startStream() {
    APP_DBG("startStream: Checking if avStream has a value...\n");
    if (avStream.has_value()) {
        APP_DBG("startStream: avStream already has a value, exiting function.\n");
        return;
    }

    APP_DBG("startStream: avStream does not have a value, creating new stream...\n");
    shared_ptr<Stream> stream = createStream();
    avStream = stream;
    APP_DBG("startStream: New stream created and assigned to avStream.\n");

    stream->start();
    APP_DBG("startStream: Stream started successfully.\n");
}

shared_ptr<Stream> createStream() {
    APP_DBG("Starting createStream function.\n");

    // Initialize encoding settings
    mtce_encode_t encodeSetting;
    int fps = LIVE_VIDEO_FPS;

    if (mtce_configGetEncode(&encodeSetting) == APP_CONFIG_SUCCESS) {
        fps = encodeSetting.mainFmt.format.FPS;
        APP_DBG("Encoding settings loaded successfully. FPS: %d\n", fps);
    } else {
        APP_DBG("Failed to load encoding settings. Using default FPS: %d\n", fps);
    }

    // Create video and audio sources for live streaming
    auto videoLive = make_shared<H26XSource>(fps);
    auto audioLive = make_shared<AudioSource>(LIVE_AUDIO_SPS);
    auto mediaLive = make_shared<MediaStream>(videoLive, audioLive);
    APP_DBG("Live video and audio sources created successfully.\n");

    // Create video and audio sources for playback
    auto videoPLayback = make_shared<H26XSource>(PLAYBACK_VIDEO_FPS);
    auto audioPLayback = make_shared<AudioSource>(PLAYBACK_AUDIO_SPS);
    auto mediaPLayback = make_shared<MediaStream>(videoPLayback, audioPLayback);
    APP_DBG("Playback video and audio sources created successfully.\n");

    // Create the stream object with live and playback media streams
    auto stream = make_shared<Stream>(mediaLive, mediaPLayback);
    APP_DBG("Stream object created successfully.\n");

    // Set the callback responsible for sample sending
    stream->onPbSampleHdl([ws = make_weak_ptr(stream)](StreamSourceType type, uint64_t sampleTime) {
        APP_DBG("Entered onPbSampleHdl callback. StreamSourceType: %d, SampleTime: %llu\n", type, sampleTime);

        bool isClientsWatchRecordExisted = false;
        auto wsl = ws.lock();

        if (!wsl) {
            APP_DBG("Weak pointer to stream expired.\n");
            return;
        }

        lockMutexListClients();
        APP_DBG("Locked client list mutex.\n");

        for (auto it : clients) {
            auto id = it.first;
            auto client = it.second;
            auto optTrackData = (type == StreamSourceType::Video) ? client->video : client->audio;

            if (client->getMediaStreamOptions() != Client::eOptions::Playback) {
                continue;
            }

            isClientsWatchRecordExisted = true;

            if (client->getPbStatus() == PlayBack::ePbStatus::Playing) {
                auto trackData = optTrackData.value();
                auto rtpConfig = trackData->sender->rtpConfig;

                SDSource* pSDSource = (type == StreamSourceType::Video) ? client->getVideoPbAttributes() : client->getAudioPbAttributes();
                wsl->mediaPLayback->loadNextSample(pSDSource, type);
                auto samplesSend = wsl->mediaPLayback->getSample(type);
                APP_DBG("Loaded next sample for client: %s, Sample count: %zu\n", id.c_str(), samplesSend.size());

                auto elapsedSeconds = double(wsl->mediaPLayback->getSampleTime_us(type)) / (1000 * 1000);
                uint32_t elapsedTimestamp = rtpConfig->secondsToTimestamp(elapsedSeconds);
                rtpConfig->timestamp = rtpConfig->startTimestamp + elapsedTimestamp;
                auto reportElapsedTimestamp = rtpConfig->timestamp - trackData->sender->lastReportedTimestamp();

                if (rtpConfig->timestampToSeconds(reportElapsedTimestamp) > 1) {
                    trackData->sender->setNeedsToReport();
                }

                if (client->getPbTimeSpentInSecs() != pSDSource->lastSecondsTick) {
                    pSDSource->lastSecondsTick = client->getPbTimeSpentInSecs();

                    json JSON;
                    JSON["Id"] = mtce_getSerialInfo();
                    JSON["Command"] = "Playback";
                    JSON["Type"] = "Report";
                    JSON["Content"]["SeekPos"] = client->getPbTimeSpentInSecs();
                    JSON["Content"]["Status"] = client->getPbStatus();

                    if (type == StreamSourceType::Video) {
                        auto dc = client->dataChannel.value();
                        try {
                            if (dc->isOpen()) {
                                dc->send(JSON.dump());
                                APP_DBG("Sent playback report to client: %s\n", id.c_str());
                            }
                        } catch (const exception& error) {
                            APP_DBG("Exception sending playback report to client %s: %s\n", id.c_str(), error.what());
                        }
                    }
                }

                try {
                    trackData->track->send(samplesSend);
                    APP_DBG("Sent samples to track for client: %s\n", id.c_str());
                } catch (const exception& error) {
                    APP_DBG("Exception sending samples to track for client %s: %s\n", id.c_str(), error.what());
                }

                wsl->mediaPLayback->rstSample(type);
                APP_DBG("Reset samples for client: %s\n", id.c_str());
            }
        }

        unlockMutexListClients();
        APP_DBG("Unlocked client list mutex.\n");

        if (!isClientsWatchRecordExisted) {
            APP_DBG("[MESSAGE] Pending Playback Session.\n");
            wsl->pendingPbSession();
        }
    });

    APP_DBG("createStream function completed.\n");
    return stream;
}

shared_ptr<Client> createPeerConnection(const Configuration &rtcConfig, weak_ptr<WebSocket> wws, const string& id) {
    if (wws.expired()) {
        APP_DBG("WebSocket pointer expired before creating PeerConnection for client ID: %s\n", id.c_str());
        return nullptr;
    }

    auto ws = wws.lock();
    APP_DBG("[WebSocket: ALIVE]\n");

    APP_DBG("RTC Configuration for Client ID: %s\n", id.c_str());
    for (const auto& server : rtcConfig.iceServers) {
        APP_DBG("\tHostname: %s, Port: %d, Username: %s, Password: %s, RelayType: %d\n",
                server.hostname.c_str(), server.port, server.username.c_str(), server.password.c_str(), static_cast<int>(server.relayType));
    }

    APP_DBG("Attempting to create PeerConnection...\n");
    shared_ptr<PeerConnection> pc;
    try {
        pc = make_shared<PeerConnection>(rtcConfig);
        if (!pc) {
            throw runtime_error("Failed to instantiate PeerConnection.");
        }
    } catch (const std::exception& e) {
        APP_DBG("[ERROR] Failed to create PeerConnection for Client ID: %s: %s\n", id.c_str(), e.what());
        return nullptr; 
    }
    APP_DBG("PeerConnection successfully created for Client ID: %s.\n", id.c_str());

    auto client = make_shared<Client>(pc);
    client->setId(id);

    pc->onStateChange([id](PeerConnection::State state) {
        APP_DBG("State: %d\n", static_cast<int>(state));
        if (state == PeerConnection::State::Disconnected || state == PeerConnection::State::Failed || state == PeerConnection::State::Closed) {
            APP_DBG("call erase client from lib\n");
            systemTimer.add(milliseconds(100),
                            [id](CppTime::timer_id) { task_post_dynamic_msg(GW_TASK_WEBRTC_ID, GW_WEBRTC_ERASE_CLIENT_REQ, (uint8_t *)id.c_str(), id.length() + 1); });
        }
    });

    pc->onGatheringStateChange([wpc = make_weak_ptr(pc), id, wws](PeerConnection::GatheringState state) {
        APP_DBG("[onGatheringStateChange] %d for Client ID: %s\n", static_cast<int>(state), id.c_str());
        if (state == PeerConnection::GatheringState::Complete) {
            auto pc = wpc.lock();
            if (!pc) {
                APP_DBG("PeerConnection weak_ptr expired for client: %s.\n", id.c_str());
                return;
            }

            auto description = pc->localDescription();
            if (!description) {
                APP_DBG("Failed to get local description for client: %s.\n", id.c_str());
                return;
            }

            json message = {
                {"ClientId", id},
                {"Type", description->typeString()},
                {"Sdp", string(description.value())}
            };

            APP_DBG("[BEFORE] Sent SDP to WebSocket: %s\n", message.dump().c_str());

            if (auto ws = wws.lock()) {
                try {
                    if (ws->isOpen()) {
                        ws->send(message.dump());
                        APP_DBG("Sent SDP to WebSocket for client: %s.\n", id.c_str());
                    } else {
                        APP_DBG("WebSocket is not open for client: %s.\n", id.c_str());
                    }
                } catch (const std::exception& e) {
                    APP_DBG("Exception while sending message for client: %s: %s\n", id.c_str(), e.what());
                }
            } else {
                APP_DBG("WebSocket weak_ptr expired for client: %s.\n", id.c_str());
            }
        }
    });

    pc->onLocalCandidate([id, wpc = make_weak_ptr(pc)](const Candidate &candidate) {
        APP_DBG("Entered onLocalCandidate callback for client: %s.\n", id.c_str());
        if (wpc.expired()) {
            APP_DBG("PeerConnection weak pointer expired for client: %s.\n", id.c_str());
            return;
        }
        
        auto lockedPc = wpc.lock();
        if (!lockedPc) {
            APP_DBG("Failed to lock PeerConnection for client: %s.\n", id.c_str());
            return;
        }
        
        APP_DBG("PeerConnection locked successfully for client: %s.\n", id.c_str());
        
        json message = {
            {"Type", "candidate"},
            {"Candidate", candidate.candidate()},
            {"SdpMid", candidate.mid()},
            {"ClientId", id}
        };
        std::cout << message.dump(4) << std::endl;

        std::string message_str = message.dump();
        APP_DBG("JSON message constructed for client: %s, message: %s\n", id.c_str(), message_str.c_str());

        try {
            task_post_dynamic_msg(GW_TASK_HELLO_ID, GW_WEBRTC_ICE_CANDIDATE, (uint8_t *)message_str.data(), message_str.length() + 1);
            APP_DBG("ICE candidate message posted successfully for client: %s.\n", id.c_str());
        } catch (const std::exception& e) {
            APP_DBG("Exception caught while posting ICE candidate message for client: %s: %s\n", id.c_str(), e.what());
        }
    });

    client->video = addVideo(pc, 102, 1, "VideoStream", "Stream", [id, wc = make_weak_ptr(client)]() {
        APP_DBG("[addVideo] open addVideo label\n");
        if (auto c = wc.lock()) {
            addToStream(c, true);
        }
        APP_DBG("Video from %s opened\n", id.c_str());
    });

    client->audio = addAudio(pc, 8, 2, "AudioStream", "Stream", [id, wc = make_weak_ptr(client)]() {
        APP_DBG("[addAudio] open addAudio label\n");
        if (auto c = wc.lock()) {
            addToStream(c, false);
        }
        APP_DBG("Audio from %s opened\n", id.c_str());
    }, id);

    auto dc = pc->createDataChannel("control");
    dc->onOpen([id, wcl = make_weak_ptr(client)]() {
        APP_DBG("[createDataChannel] open channel label\n");
        if (auto cl = wcl.lock()) {
            auto dc = cl->dataChannel.value();
            APP_DBG("open channel label: %s success\n", dc->label().c_str());
            cl->removeTimeoutConnect();
            cl->mIsSignalingOk = true;
            if (++Client::totalClientsConnectSuccess > CLIENT_MAX) {
                APP_DBG("Client::totalClientsConnectSuccess > %d\n", CLIENT_MAX);
                FATAL("RTC", 0x01);
            }
            APP_DBG("total client: %d\n", Client::totalClientsConnectSuccess.load());

            auto pc = cl->peerConnection;
            Candidate iceCam, iceApp;
            if (pc->getSelectedCandidatePair(&iceCam, &iceApp)) {
                APP_DBG("local candidate selected: %s, transport type: %d\n", iceCam.candidate().c_str(), iceCam.transportType());
                APP_DBG("remote candidate selected: %s, transport type: %d\n", iceApp.candidate().c_str(), iceApp.transportType());
            } else {
                APP_DBG("get selected candidate pair failed\n");
            }
        }
    });

    dc->onMessage([id](auto data) {
        if (holds_alternative<string>(data)) {
            json dataRev = {
                {"ClientId", id},
                {"Data", get<string>(data)}
            };
            task_post_dynamic_msg(GW_TASK_WEBRTC_ID, GW_WEBRTC_ON_MESSAGE_CONTROL_DATACHANNEL_REQ, (uint8_t *)dataRev.dump().c_str(), dataRev.dump().length() + 1);
        } else {
            APP_DBG("Binary message from %s received, size= %d\n", id.c_str(), get<rtc::binary>(data).size());
        }
    });

    dc->onBufferedAmountLow([id]() { APP_DBG("clientId %s send done\n", id.c_str()); });
    client->dataChannel = dc;

    return client;
}


shared_ptr<ClientTrackData> addVideo(const shared_ptr<PeerConnection> pc, const uint8_t payloadType, const uint32_t ssrc, const string cname, const string msid, const function<void(void)> onOpen) {
    auto video = Description::Video(cname, Description::Direction::SendOnly);
    video.addH264Codec(payloadType);
    video.addSSRC(ssrc, cname, msid, cname);
    auto track = pc->addTrack(video);

    // Create RTP configuration
    auto rtpConfig = make_shared<RtpPacketizationConfig>(ssrc, cname, payloadType, H264RtpPacketizer::defaultClockRate);

    // Create packetizer
    auto packetizer = make_shared<H264RtpPacketizer>(NalUnit::Separator::Length, rtpConfig);

    // Add RTCP SR handler
    auto srReporter = make_shared<RtcpSrReporter>(rtpConfig);
    packetizer->addToChain(srReporter);

    // Add RTCP NACK handler
    auto nackResponder = make_shared<RtcpNackResponder>();
    packetizer->addToChain(nackResponder);

    // Set handler
    track->setMediaHandler(packetizer);
    track->onOpen(onOpen);

    auto trackData = make_shared<ClientTrackData>(track, srReporter);
    return trackData;
}


shared_ptr<ClientTrackData> addAudio(const shared_ptr<PeerConnection> pc, const uint8_t payloadType, const uint32_t ssrc, const string cname, const string msid, const function<void(void)> onOpen, std::string id) {
    auto audio = Description::Audio(cname, Description::Direction::SendRecv);
    audio.addPCMACodec(payloadType);
    audio.addSSRC(ssrc, cname, msid, cname);
    auto track = pc->addTrack(audio);

    // Create RTP configuration
    auto rtpConfig = make_shared<RtpPacketizationConfig>(ssrc, cname, payloadType, ALAWRtpPacketizer::DefaultClockRate);

    // Create packetizer
    auto packetizer = make_shared<ALAWRtpPacketizer>(rtpConfig);

    // Add RTCP SR handler
    auto srReporter = make_shared<RtcpSrReporter>(rtpConfig);
    packetizer->addToChain(srReporter);

    // Add RTCP NACK handler
    auto nackResponder = make_shared<RtcpNackResponder>();
    packetizer->addToChain(nackResponder);

    // Set handler
    track->setMediaHandler(packetizer);

    // Uncomment and adjust if you need to handle push-to-talk functionality
    /*
    track->onMessage(
        [id](rtc::binary message) {
            if (Client::currentClientPushToTalk.empty() == true || id != Client::currentClientPushToTalk) {
                return;
            }

            if (message.size() < sizeof(rtc::RtpHeader)) {
                return;
            }

            // This is an RTP packet
            auto rtpHdr = reinterpret_cast<rtc::RtpHeader *>(message.data());
            char *rtpBody = rtpHdr->getBody();
            size_t bodyLength = message.size() - rtpHdr->getSize();

            if (bodyLength < 160) { // This is not audio samples
                return;
            }

            audioSpeakerBuffers.insert(audioSpeakerBuffers.end(), rtpBody, rtpBody + bodyLength);

            if (audioSpeakerBuffers.size() > 512) {
                pushToTalkThread.dispatch([buffer = std::move(audioSpeakerBuffers)]() mutable {
                    AudioChannel::setStopPlayFile(true);
                    int sent = AudioChannel::playSpeaker(buffer.data(), buffer.size());
                    APP_DBG("Sent audio size %d\n", sent);
                    buffer.clear();
                    AudioChannel::setStopPlayFile(false);
                });
            }

            timer_set(GW_TASK_WEBRTC_ID, GW_WEBRTC_RELEASE_CLIENT_PUSH_TO_TALK, GW_WEBRTC_RELEASE_CLIENT_PUSH_TO_TALK_INTERVAL, TIMER_ONE_SHOT);
        },
        nullptr);
    */

    track->onOpen(onOpen);

    auto trackData = make_shared<ClientTrackData>(track, srReporter);
    return trackData;
}




void periodicClientCheck() {
    for (const auto& clientPair : clients) {
        auto client = clientPair.second;
        if (client) {
            if (!client->isAlive()) {
                APP_DBG("Client with ID: %s is no longer alive.\n", clientPair.first.c_str());
                APP_DBG("PeerConnection state for Client ID: %s is not active.\n", clientPair.first.c_str());
                // Handle client disconnection or cleanup
            } else {
                APP_DBG("Client with ID: %s is still alive.\n", clientPair.first.c_str());
            }
        } else {
            APP_DBG("Client with ID: %s is null.\n", clientPair.first.c_str());
        }
    }
}

void startPeriodicCheck() {
    std::thread([]() {
        while (true) {
            periodicClientCheck();
            std::this_thread::sleep_for(std::chrono::seconds(3)); // Check every 10 seconds
        }
    }).detach();
}

//WebSocket Initialization and Management
void initializeWebSocketServer(const std::string &wsUrl) {
    globalWebSocket = std::make_shared<WebSocket>();

    globalWebSocket->onOpen([&]() {
        isConnected.store(true);
        APP_DBG("WebSocket connected, signaling ready\n");
        timer_remove_attr(GW_TASK_WEBRTC_ID, GW_WEBRTC_TRY_CONNECT_SOCKET_REQ);
    });

    globalWebSocket->onClosed([&]() {
        isConnected.store(false);
        APP_DBG("WebSocket closed\n");
        timer_set(GW_TASK_WEBRTC_ID, GW_WEBRTC_TRY_CONNECT_SOCKET_REQ, GW_WEBRTC_TRY_CONNECT_SOCKET_INTERVAL, TIMER_ONE_SHOT);
    });

    globalWebSocket->onError([&](const string &error) {
        isConnected.store(false);
        APP_DBG("WebSocket connection failed: %s\n", error.c_str());
    });

    globalWebSocket->onMessage([&](variant<binary, string> data) {
        APP_DBG("WebSocket connection @ ws->onMessage\n");
        if (holds_alternative<string>(data)) {
            string msg = get<string>(data);
            APP_DBG("%s\n", msg.data());
            handleWebSocketMessage(msg, globalWebSocket);
        }
    });

    globalWebSocket->open(wsUrl);
}

//Configuration Loading
//done
int8_t loadIceServersConfigFile(Configuration &rtcConfig) {
    rtcServersConfig_t rtcServerCfg;
    int8_t ret = configGetRtcServers(&rtcServerCfg);
    try {
        if (ret == APP_CONFIG_SUCCESS) {
            rtcConfig.iceServers.clear();

            APP_DBG("List stun server:\n");
            for (const auto& url : rtcServerCfg.arrStunServerUrl) {
                if (!url.empty()) {
                    rtcConfig.iceServers.emplace_back(url);
                    APP_DBG("\turl: %s\n", url.c_str());
                }
            }
            APP_DBG("\nList turn server:\n");
            for (const auto& url : rtcServerCfg.arrTurnServerUrl) {
                if (!url.empty()) {
                    rtcConfig.iceServers.emplace_back(url);
                    APP_DBG("\turl: %s\n", url.c_str());
                }
            }
            APP_DBG("\n");
        }
    } catch (const exception &error) {
        APP_DBG("loadIceServersConfigFile %s\n", error.what());
        ret = APP_CONFIG_ERROR_DATA_INVALID;
    }
    return ret;
}

int8_t loadWsocketSignalingServerConfigFile(string &wsUrl) {
    rtcServersConfig_t rtcServerCfg;
    int8_t ret = configGetRtcServers(&rtcServerCfg);

    if (ret == APP_CONFIG_SUCCESS) {
        wsUrl.clear();
        if (!rtcServerCfg.wSocketServerCfg.empty()) {
            wsUrl = rtcServerCfg.wSocketServerCfg + "/" + mtce_getSerialInfo();
            APP_DBG("[DEBUG] WebSocket URL constructed: %s\n", wsUrl.c_str());
        } else {
            APP_DBG("[ERROR] WebSocket server configuration is empty.\n");
        }
    } else {
        APP_DBG("[ERROR] Failed to retrieve RTC server configuration. Error code: %d\n", ret);
    }

    return ret;
}
//done

//WebSocket Message Handling
void handleWebSocketMessage(const std::string& message, std::shared_ptr<WebSocket> ws) {
    APP_DBG("[WebSocket Message Received]: %s\n", message.c_str());
    if (ws && ws->isOpen()) {
        APP_DBG("safe to use\n");
    } else {
        APP_DBG("WebSocket is not open or not available.\n");
    }

    try {
        json messageJson = json::parse(message);
        APP_DBG("Parsed WebSocket Message: %s\n", messageJson.dump(4).c_str());

        auto typeIt = messageJson.find("Type");
        if (typeIt != messageJson.end()) {
            string type = typeIt->get<string>();

            if (type == "request") {
                auto clientIdIt = messageJson.find("ClientId");
                if (clientIdIt != messageJson.end()) {
                    string clientId = clientIdIt->get<string>();
                    handleClientRequest(clientId, ws);
                } else {
                    APP_DBG("Error: ClientId not found in request message\n");
                }
            } else if (type == "candidate") {
                APP_DBG("[Type: candidate]\n");
                string sdp = messageJson["sdp"].get<string>();
                string mid = messageJson["mid"].get<string>();
                APP_DBG("Adding ICE Candidate: sdp=%s, mid=%s\n", sdp.c_str(), mid.c_str());
                // pc->addRemoteCandidate(Candidate(sdp, mid));
            } else if (type == "answer") {
                auto clientIdIt = messageJson.find("ClientId");
                if (clientIdIt != messageJson.end()) {
                    string clientId = clientIdIt->get<string>();
                    handleAnswer(clientId, messageJson);
                }
            }
        } else {
            APP_DBG("Error: Message type not specified\n");
        }
    } catch (const json::exception& e) {
        APP_DBG("[JSON Parsing Error] %s\n", e.what());
    }
}

//Client Request Handling
void handleClientRequest(const std::string& clientId, std::shared_ptr<WebSocket> ws) {
    APP_DBG("Initiating peer connection for Client ID: %s\n", clientId.c_str());

    if (Client::totalClientsConnectSuccess <= CLIENT_MAX && clients.size() <= CLIENT_SIGNALING_MAX) {
        Client::setSignalingStatus(true);
        std::shared_ptr<Client> newClient = createPeerConnection(rtcConfig, make_weak_ptr(ws), clientId);
        peerConnections[clientId] = newClient->peerConnection;
        clients[clientId] = newClient;

        lockMutexListClients();
        clients.emplace(clientId, newClient);
        auto cl = clients.at(clientId);
        cl->startTimeoutConnect();
        APP_DBG("Started timeout connect for client: %s\n", clientId.c_str());
        unlockMutexListClients();
        Client::setSignalingStatus(false);
    } else {
        APP_DBG("Reached max client limit. Cannot handle new request for client: %s\n", clientId.c_str());
    }
}

void handleAnswer(const std::string& id, const json& message) {
    auto pcIter = peerConnections.find(id);
    if (pcIter == peerConnections.end()) {
        APP_DBG("No PeerConnection found for client ID: %s\n", id.c_str());
        return;
    }
    std::shared_ptr<PeerConnection>& pc = pcIter->second;
    if (!pc) {
        APP_DBG("PeerConnection pointer is null, cannot handle answer for client ID: %s\n", id.c_str());
        return;
    }
    auto sdp = message["Sdp"].get<string>();
    auto desRev = Description(sdp, "answer");

    if (pc->remoteDescription().has_value()) {
        if (desRev.iceUfrag().has_value() && desRev.icePwd().has_value() &&
            desRev.iceUfrag().value() == pc->remoteDescription().value().iceUfrag().value() &&
            desRev.icePwd().value() == pc->remoteDescription().value().icePwd().value()) {

            auto remoteCandidates = desRev.extractCandidates();
            for (const auto& candidate : remoteCandidates) {
                APP_DBG("Adding ICE Candidate: %s\n", candidate.candidate().c_str());
                pc->addRemoteCandidate(candidate);
            }
        } else {
            APP_DBG("ICE credentials mismatch for client ID: %s\n", id.c_str());
        }
    } else {
        try {
            pc->setRemoteDescription(desRev);
            APP_DBG("New session description set for client ID: %s\n", id.c_str());
        } catch (const exception& e) {
            APP_DBG("Error setting remote description for client ID: %s: %s\n", id.c_str(), e.what());
        }
    }
}

#pragma once

#include <atomic>
#include <set>
#include "hu_aap.h"

#include "command_server.h"
#include "audio.h"
#include <dbus-c++/dbus.h>
#include <asoundlib.h>

#include "dbus/generated_cmu.h"

class VideoOutput;
class AudioOutput;
class MazdaEventCallbacks;
class VideoManagerClient;

class NativeGUICtrlClient : public com::jci::nativeguictrl_proxy,
                            public DBus::ObjectProxy
{
public:
    NativeGUICtrlClient(DBus::Connection &connection)
            : DBus::ObjectProxy(connection, "/com/jci/nativeguictrl", "com.jci.nativeguictrl")
    {
    }

    enum SURFACES
    {
        NNG_NAVI_ID = 0,
        TV_TOUCH_SURFACE,
        NATGUI_SURFACE,
        LOOPLOGO_SURFACE,
        TRANLOGOEND_SURFACE,
        TRANLOGO_SURFACE,
        QUICKTRANLOGO_SURFACE,
        EXITLOGO_SURFACE,
        JCI_OPERA_PRIMARY,
        JCI_OPERA_SECONDARY,
        lvdsSurface,
        SCREENREP_IVI_NAME,
        NNG_NAVI_MAP1,
        NNG_NAVI_MAP2,
        NNG_NAVI_HMI,
        NNG_NAVI_TWN,
    };

    void SetRequiredSurfacesByEnum(const std::vector<SURFACES>& surfaces, bool fadeOpera)
    {
        std::ostringstream idString;
        for (size_t i = 0; i < surfaces.size(); i++)
        {
            if (i > 0)
                idString << ",";
            idString << surfaces[i];
        }
        SetRequiredSurfaces(idString.str(), fadeOpera ? 1 : 0);
    }
};

class AudioManagerClient : public com::xsembedded::ServiceProvider_proxy,
                     public DBus::ObjectProxy
{
public:
    enum class FocusType
    {
        NONE,
        PERMANENT,
        TRANSIENT,
    };
private:
    std::map<std::string, int> streamToSessionIds;
    std::string aaStreamName = "MLENT";
    int aaSessionID = -1;
    int aaTransientSessionID = -1;
    int previousSessionID = -1;
    bool waitingForFocusLostEvent = false;
    MazdaEventCallbacks& callbacks;
    FocusType currentFocus = FocusType::NONE;

    //These IDs are usually the same, but they depend on the startup order of the services on the car so we can't assume them 100% reliably
    void populateStreamTable();
    void aaRegisterStream();
public:
    AudioManagerClient(MazdaEventCallbacks& callbacks, DBus::Connection &connection);
    ~AudioManagerClient();

    bool canSwitchAudio();

    //calling requestAudioFocus directly doesn't work on the audio mgr
    void audioMgrRequestAudioFocus(FocusType type);
    void audioMgrReleaseAudioFocus();

    virtual void Notify(const std::string& signalName, const std::string& payload) override;
};

class BTHFClient : public com::jci::bthf_proxy,
        public DBus::ObjectProxy
{
    MazdaEventCallbacks& callbacks;
public:
    BTHFClient(MazdaEventCallbacks& callbacks, DBus::Connection &hmiBus);

    enum class CallStatusState : uint32_t
    {
       Inactive = 0,
       InCall,
       Incoming,
       OnHold,
       Muted,
       Conferenced,
    };

    virtual void CallStatus(const uint32_t& bthfstate, const uint32_t& call1status, const uint32_t& call2status, const ::DBus::Struct< std::vector< uint8_t > >& call1Number, const ::DBus::Struct< std::vector< uint8_t > >& call2Number) override;
    virtual void BatteryIndicator(const uint32_t& minValue, const uint32_t& maxValue, const uint32_t& currentValue)  override { }
    virtual void SignalStrength(const uint32_t& minValue, const uint32_t& maxValue, const uint32_t& currentValue)  override { }
    virtual void RoamIndicator(const uint32_t& value)  override { }
    virtual void NewServiceIndicator(const bool& value)  override { }
    virtual void PhoneChargeIndicator(const uint32_t& value)  override { }
    virtual void SmsPresentIndicator(const bool& value)  override { }
    virtual void VoiceMailIndicator(const bool& value)  override { }
    virtual void LowBatteryIndicator(const bool& value)  override { }
    virtual void BthfReadyStatus(const uint32_t& hftReady, const uint32_t& reasonCode)  override { }
    virtual void BthfBusyReason(const uint32_t& busyReason)  override { }
    virtual void MicStatus(const bool& isMicMuted)  override { }
    virtual void BargeinStatus(const bool& isBargeinActive)  override { }
    virtual void BthfSettingsResponse(const ::DBus::Struct< std::vector< uint8_t > >& callsettings)  override { }
    virtual void FailureReasonCodes(const uint32_t& errorType)  override { }
};

enum class VIDEO_FOCUS_REQUESTOR : u_int8_t {
    HEADUNIT, // headunit (we) has requested video focus
    ANDROID_AUTO, // AA phone app has requested video focus
    BACKUP_CAMERA // CMU requested screen for backup camera
};

class VideoManagerClient : public com::jci::bucpsa_proxy,
                           public DBus::ObjectProxy {
    bool allowedToGetFocus = true;
    bool waitsForFocus = false;

    MazdaEventCallbacks& callbacks;
    NativeGUICtrlClient guiClient;
public:
    VideoManagerClient(MazdaEventCallbacks& callbacks, DBus::Connection &hmiBus);
    ~VideoManagerClient();

    void requestVideoFocus(VIDEO_FOCUS_REQUESTOR requestor);
    void releaseVideoFocus(VIDEO_FOCUS_REQUESTOR requestor);

    virtual void CommandResponse(const uint32_t& cmdResponse) override {}
    virtual void DisplayMode(const uint32_t& currentDisplayMode) override;
    virtual void ReverseStatusChanged(const int32_t& reverseStatus) override {}
    virtual void PSMInstallStatusChanged(const uint8_t& psmInstalled) override {}
};

class MazdaEventCallbacks : public IHUConnectionThreadEventCallbacks {
    std::unique_ptr<VideoOutput> videoOutput;
    std::unique_ptr<AudioOutput> audioOutput;
    std::unique_ptr<BTHFClient> phoneStateClient;

    MicInput micInput;
    DBus::Connection& serviceBus;
    DBus::Connection& hmiBus;

    std::unique_ptr<AudioManagerClient> audioMgrClient;
    std::unique_ptr<VideoManagerClient> videoMgrClient;
public:
    MazdaEventCallbacks(DBus::Connection& serviceBus, DBus::Connection& hmiBus);
    ~MazdaEventCallbacks();

    virtual int MediaPacket(int chan, uint64_t timestamp, const byte * buf, int len) override;
    virtual int MediaStart(int chan) override;
    virtual int MediaStop(int chan) override;
    virtual void MediaSetupComplete(int chan) override;
    virtual void DisconnectionOrError() override;
    virtual void CustomizeOutputChannel(int chan, HU::ChannelDescriptor::OutputStreamChannel& streamChannel) override;
    virtual void AudioFocusRequest(int chan, const HU::AudioFocusRequest& request) override;
    virtual void VideoFocusRequest(int chan, const HU::VideoFocusRequest& request) override;

    virtual std::string GetCarBluetoothAddress() override;

    void takeVideoFocus();
    void releaseVideoFocus();
    void releaseAudioFocus();

    void VideoFocusHappened(bool hasFocus, bool unrequested);
    void AudioFocusHappend(AudioManagerClient::FocusType type);

    std::atomic<bool> connected;
    std::atomic<bool> videoFocus;
    std::atomic<bool> inCall;
    std::atomic<AudioManagerClient::FocusType> audioFocus;
};

class MazdaCommandServerCallbacks : public ICommandServerCallbacks
{
public:
    MazdaCommandServerCallbacks();

    MazdaEventCallbacks* eventCallbacks = nullptr;

    virtual bool IsConnected() const override;
    virtual bool HasAudioFocus() const override;
    virtual bool HasVideoFocus() const override;
    virtual void TakeVideoFocus() override;
    virtual std::string GetLogPath() const override;
};

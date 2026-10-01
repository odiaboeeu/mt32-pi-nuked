//
// nukedmt32synth.h
//

#ifndef _nukedmt32synth_h
#define _nukedmt32synth_h

#include <circle/types.h>

#include "FloatSampleProvider.h"
#include "rommanager.h"
#include "synth/mt32romset.h"
#include "synth/nukedmt32romversion.h"
#include "synth/synthbase.h"

class mt32_t;
class Mt32Reverb;

class CNukedMT32Synth
    : public CSynthBase,
      private SRCTools::FloatSampleProvider
{
public:
    explicit CNukedMT32Synth(unsigned int nSampleRate);
    virtual ~CNukedMT32Synth();

    virtual bool Initialize() override;
    virtual void HandleMIDIShortMessage(u32 nMessage) override;
    virtual void HandleMIDISysExMessage(
        const u8* pData,
        size_t nSize
    ) override;
    virtual bool IsActive() override;
    virtual void AllSoundOff() override;
    virtual void HandleActiveSenseTimeout() override;
    virtual void SetMasterVolume(u8 nVolume) override;
    virtual size_t Render(s16* pOutBuffer, size_t nFrames) override;
    virtual size_t Render(float* pOutBuffer, size_t nFrames) override;
    virtual void ReportStatus() const override;
    virtual void UpdateLCD(CLCD& LCD, unsigned int nTicks) override;

    void SetMIDIChannels(bool bAlternate);
    void SetReversedStereo(bool bEnabled)
    {
        m_bReversedStereo = bEnabled;
    }

    TMT32ROMSet GetROMSet() const
    {
        return m_CurrentROMSet;
    }

    CROMManager& GetROMManager()
    {
        return m_ROMManager;
    }

private:
    static constexpr unsigned int NativeSampleRate = 32000;
    static constexpr size_t NativeBufferFrames = 8192;
    static constexpr size_t ConversionBufferFrames = 256;
    static constexpr size_t OldControlROMSize = 0x10000;
    static constexpr size_t NewControlROMSize = 0x20000;
    static constexpr size_t PCMROMSize = 0x80000;
    static constexpr size_t LCDTextLength = 20;
    static constexpr size_t MT32PartCount = 9;

    static unsigned int GetShortMessageLength(u8 nStatus);

    void PostMIDIByte(u8 nByte);

    virtual void getOutputSamples(
        float* pOutBuffer,
        unsigned int nFrames
    ) override;

    void ClearSynth();

    mt32_t* m_pMT32;
    Mt32Reverb* m_pReverb;
    SRCTools::FloatSampleProvider* m_pResamplerModel;

    CROMManager m_ROMManager;
    TMT32ROMSet m_CurrentROMSet;
    const MT32Emu::ROMImage* m_pControlROMImage;
    const MT32Emu::ROMImage* m_pPCMROMImage;

    u8 m_nMasterVolume;
    bool m_bInitialized;
    bool m_bReversedStereo;

    u8 m_MIDIChannelPartMap[MT32PartCount];
    char m_LCDText[LCDTextLength + 1];
};

#endif

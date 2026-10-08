//
// nukedmt32synth.cpp
//

#include <circle/logger.h>

#include <cstring>

#include "config.h"
#include "mt32.h"
#include "ResamplerModel.h"
#include "reverb.h"
#include "synth/nukedmt32synth.h"

LOGMODULE("nukedmt32synth");

CNukedMT32Synth::CNukedMT32Synth(unsigned int nSampleRate)
    : CSynthBase(nSampleRate),
      m_bDCBlock(false),
      m_pMT32(nullptr),
      m_pReverb(nullptr),
      m_pResamplerModel(nullptr),
      m_CurrentROMSet(TMT32ROMSet::Any),
      m_pControlROMImage(nullptr),
      m_pPCMROMImage(nullptr),
      m_nMasterVolume(100),
      m_bInitialized(false),
      m_bReversedStereo(false),
      m_MIDIChannelPartMap{
          0x01,
          0x02,
          0x03,
          0x04,
          0x05,
          0x06,
          0x07,
          0x08,
          0x09
      },
      m_LCDText{'\0'}
{
}

CNukedMT32Synth::~CNukedMT32Synth()
{
    ClearSynth();
}

void CNukedMT32Synth::ClearSynth()
{
    m_DcBlocker.reset();
    if (m_pResamplerModel)
    {
        SRCTools::ResamplerModel::freeResamplerModel(
            *m_pResamplerModel,
            *this
        );

        m_pResamplerModel = nullptr;
    }

    delete m_pReverb;
    m_pReverb = nullptr;

    delete m_pMT32;
    m_pMT32 = nullptr;

    m_bInitialized = false;
}

bool CNukedMT32Synth::Initialize()
{
    if (!m_ROMManager.ScanROMs())
    {
        LOGERR("No MT-32 ROM set available");
        return false;
    }

    const TNukedMT32ROMVersion ROMVersion =
        CConfig::Get()->NukedMT32ROMVersion;

    if (!m_ROMManager.GetNukedMT32ROMSet(
            ROMVersion,
            m_pControlROMImage,
            m_pPCMROMImage))
    {
        LOGERR(
            "Requested Nuked-MT32 Control ROM unavailable"
        );
        return false;
    }

    m_CurrentROMSet =
        ROMVersion <= TNukedMT32ROMVersion::V1_07
            ? TMT32ROMSet::MT32Old
            : TMT32ROMSet::MT32New;

    MT32Emu::File* const pControlFile =
        m_pControlROMImage->getFile();

    MT32Emu::File* const pPCMFile =
        m_pPCMROMImage->getFile();

    const size_t nExpectedControlSize =
        m_CurrentROMSet == TMT32ROMSet::MT32Old
            ? OldControlROMSize
            : NewControlROMSize;

    const size_t nControlSize = pControlFile->getSize();
    const size_t nPCMSize = pPCMFile->getSize();

    if (nControlSize != nExpectedControlSize)
    {
        LOGERR(
            "Unexpected Control ROM size: %u",
            static_cast<unsigned int>(nControlSize)
        );
        return false;
    }

    if (nPCMSize != PCMROMSize)
    {
        LOGERR(
            "Unexpected PCM ROM size: %u",
            static_cast<unsigned int>(nPCMSize)
        );
        return false;
    }

    const MT32Emu::Bit8u* const pControlData =
        pControlFile->getData();

    const MT32Emu::Bit8u* const pPCMData =
        pPCMFile->getData();

    if (!pControlData || !pPCMData)
    {
        LOGERR("MT-32 ROM data is unavailable");
        return false;
    }

    m_pMT32 = new mt32_t();

    if (!m_pMT32)
    {
        LOGERR("Failed to allocate Nuked-MT32 instance");
        return false;
    }

    std::memset(m_pMT32->rom, 0, sizeof(m_pMT32->rom));
    std::memcpy(
        m_pMT32->rom,
        pControlData,
        nControlSize
    );

    std::memcpy(
        m_pMT32->pcm,
        pPCMData,
        nPCMSize
    );

    m_pMT32->old_machine =
        m_CurrentROMSet == TMT32ROMSet::MT32Old;

    m_pReverb = new Mt32Reverb();

    if (!m_pReverb)
    {
        LOGERR("Failed to allocate Nuked-MT32 reverb");
        ClearSynth();
        return false;
    }

    m_pReverb->init();

    // Optional output DC removal; does not repair upstream clipping.
    m_bDCBlock = CConfig::Get()->NukedMT32DCBlock;
    m_DcBlocker.reset();

    const char* const pModel =
        m_pMT32->old_machine ? "MT-32 old" : "MT-32 new";

    std::strncpy(
        m_LCDText,
        pModel,
        sizeof(m_LCDText) - 1
    );
    m_LCDText[sizeof(m_LCDText) - 1] = '\0';

    m_bInitialized = true;

    m_pResamplerModel =
        &SRCTools::ResamplerModel::createResamplerModel(
            *this,
            static_cast<double>(NativeSampleRate),
            static_cast<double>(m_nSampleRate),
            SRCTools::ResamplerModel::GOOD
        );

    LOGNOTE(
        "Nuked-MT32 audio path: %u Hz to %u Hz",
        NativeSampleRate,
        m_nSampleRate
    );

    LOGNOTE(
        "Nuked-MT32 initialized with %s ROM set",
        pModel
    );

    return true;
}

unsigned int CNukedMT32Synth::GetShortMessageLength(u8 nStatus)
{
    if (nStatus >= 0xF8)
        return 1;

    if (nStatus >= 0xF0)
    {
        switch (nStatus)
        {
            case 0xF1:
            case 0xF3:
                return 2;

            case 0xF2:
                return 3;

            default:
                return 1;
        }
    }

    const u8 nType = nStatus & 0xF0;

    if (nType == 0xC0 || nType == 0xD0)
        return 2;

    return 3;
}

void CNukedMT32Synth::PostMIDIByte(u8 nByte)
{
    if (!m_bInitialized)
        return;

    m_pMT32->post_midi(nByte);
    m_pReverb->observeMidiByte(nByte);
}

void CNukedMT32Synth::HandleMIDIShortMessage(u32 nMessage)
{
    if (!m_bInitialized)
        return;

    const u8 nStatus = static_cast<u8>(nMessage);
    const unsigned int nLength =
        GetShortMessageLength(nStatus);

    m_Lock.Acquire();

    for (unsigned int i = 0; i < nLength; ++i)
        PostMIDIByte(static_cast<u8>(nMessage >> (i * 8)));

    m_Lock.Release();

    CSynthBase::HandleMIDIShortMessage(nMessage);
}

void CNukedMT32Synth::HandleMIDISysExMessage(
    const u8* pData,
    size_t nSize
)
{
    if (!m_bInitialized || !pData || nSize == 0)
        return;

    m_Lock.Acquire();

    for (size_t i = 0; i < nSize; ++i)
        PostMIDIByte(pData[i]);

    m_Lock.Release();
}

bool CNukedMT32Synth::IsActive()
{
    return m_bInitialized;
}

void CNukedMT32Synth::AllSoundOff()
{
    if (m_bInitialized)
    {
        m_Lock.Acquire();

        for (u8 nChannel = 0; nChannel < 16; ++nChannel)
        {
            PostMIDIByte(0xB0 | nChannel);
            PostMIDIByte(123);
            PostMIDIByte(0);
        }

        m_Lock.Release();
    }

    CSynthBase::AllSoundOff();
}

void CNukedMT32Synth::HandleActiveSenseTimeout()
{
    if (m_bInitialized)
    {
        m_Lock.Acquire();

        for (u8 nChannel = 0; nChannel < 16; ++nChannel)
        {
            // Restore modulation, expression, hold and pitch bend.
            PostMIDIByte(0xB0 | nChannel);
            PostMIDIByte(121);
            PostMIDIByte(0);

            // Turn off all notes activated through MIDI.
            PostMIDIByte(0xB0 | nChannel);
            PostMIDIByte(123);
            PostMIDIByte(0);
        }

        m_Lock.Release();
    }

    CSynthBase::AllSoundOff();
}

void CNukedMT32Synth::SetMIDIChannels(bool bAlternate)
{
    const u8 nAddressHigh = 0x10;
    const u8 nAddressMid = 0x00;
    const u8 nAddressLow = 0x0D;

    const u8 ChannelMap[MT32PartCount] =
    {
        static_cast<u8>(bAlternate ? 0x00 : 0x01),
        static_cast<u8>(bAlternate ? 0x01 : 0x02),
        static_cast<u8>(bAlternate ? 0x02 : 0x03),
        static_cast<u8>(bAlternate ? 0x03 : 0x04),
        static_cast<u8>(bAlternate ? 0x04 : 0x05),
        static_cast<u8>(bAlternate ? 0x05 : 0x06),
        static_cast<u8>(bAlternate ? 0x06 : 0x07),
        static_cast<u8>(bAlternate ? 0x07 : 0x08),
        0x09
    };

    unsigned int nChecksumSum =
        nAddressHigh +
        nAddressMid +
        nAddressLow;

    for (size_t i = 0; i < MT32PartCount; ++i)
        nChecksumSum += ChannelMap[i];

    const u8 nChecksum =
        static_cast<u8>(-nChecksumSum) & 0x7F;

    const u8 Header[] =
    {
        0xF0,
        0x41,
        0x10,
        0x16,
        0x12,
        nAddressHigh,
        nAddressMid,
        nAddressLow
    };

    m_Lock.Acquire();

    for (size_t i = 0; i < MT32PartCount; ++i)
        m_MIDIChannelPartMap[i] = ChannelMap[i];

    if (m_bInitialized)
    {
        for (size_t i = 0; i < sizeof(Header); ++i)
            PostMIDIByte(Header[i]);

        for (size_t i = 0; i < MT32PartCount; ++i)
            PostMIDIByte(ChannelMap[i]);

        PostMIDIByte(nChecksum);
        PostMIDIByte(0xF7);
    }

    m_Lock.Release();
}

void CNukedMT32Synth::SetMasterVolume(u8 nVolume)
{
    if (nVolume > 100)
        nVolume = 100;

    const u8 nAddressHigh = 0x10;
    const u8 nAddressMid = 0x00;
    const u8 nAddressLow = 0x16;

    const u8 nChecksum = static_cast<u8>(
        -(
            nAddressHigh +
            nAddressMid +
            nAddressLow +
            nVolume
        )
    ) & 0x7F;

    const u8 Message[] =
    {
        0xF0,
        0x41,
        0x10,
        0x16,
        0x12,
        nAddressHigh,
        nAddressMid,
        nAddressLow,
        nVolume,
        nChecksum,
        0xF7
    };

    m_Lock.Acquire();

    m_nMasterVolume = nVolume;

    if (m_bInitialized)
    {
        for (size_t i = 0; i < sizeof(Message); ++i)
            PostMIDIByte(Message[i]);
    }

    m_Lock.Release();
}

void CNukedMT32Synth::getOutputSamples(
    float* pOutBuffer,
    unsigned int nFrames
)
{
    if (!pOutBuffer)
        return;

    if (!m_bInitialized)
    {
        std::memset(
            pOutBuffer,
            0,
            static_cast<size_t>(nFrames) * 2 * sizeof(*pOutBuffer)
        );

        return;
    }

    unsigned int nRendered = 0;

    while (nRendered < nFrames)
    {
        unsigned int nChunk = nFrames - nRendered;

        if (nChunk > NativeBufferFrames)
            nChunk = NativeBufferFrames;

        m_pMT32->clock(nChunk);

        m_pReverb->process(
            &m_pMT32->samples[0][0],
            &m_pMT32->reverb_input[0][0],
            static_cast<int>(nChunk)
        );

        if (m_bDCBlock)
            m_DcBlocker.process(&m_pMT32->samples[0][0], static_cast<int>(nChunk));

        for (unsigned int i = 0; i < nChunk; ++i)
        {
            pOutBuffer[(nRendered + i) * 2] =
                static_cast<float>(
                    m_pMT32->samples[i][0]
                ) / 32768.0f;

            pOutBuffer[(nRendered + i) * 2 + 1] =
                static_cast<float>(
                    m_pMT32->samples[i][1]
                ) / 32768.0f;
        }

        nRendered += nChunk;
    }
}

size_t CNukedMT32Synth::Render(
    s16* pOutBuffer,
    size_t nFrames
)
{
    if (!pOutBuffer)
        return nFrames;

    m_Lock.Acquire();

    if (!m_bInitialized || !m_pResamplerModel)
    {
        std::memset(
            pOutBuffer,
            0,
            nFrames * 2 * sizeof(*pOutBuffer)
        );

        m_Lock.Release();
        return nFrames;
    }

    float ConversionBuffer[ConversionBufferFrames * 2];
    size_t nRendered = 0;

    while (nRendered < nFrames)
    {
        size_t nChunk = nFrames - nRendered;

        if (nChunk > ConversionBufferFrames)
            nChunk = ConversionBufferFrames;

        m_pResamplerModel->getOutputSamples(
            ConversionBuffer,
            static_cast<unsigned int>(nChunk)
        );

        for (size_t i = 0; i < nChunk * 2; ++i)
        {
            float nSample = ConversionBuffer[i];

            if (nSample > 1.0f)
                nSample = 1.0f;
            else if (nSample < -1.0f)
                nSample = -1.0f;

            const float nScaled =
                nSample >= 0.0f
                    ? nSample * 32767.0f
                    : nSample * 32768.0f;

            pOutBuffer[nRendered * 2 + i] =
                static_cast<s16>(nScaled);
        }

        nRendered += nChunk;
    }

    m_Lock.Release();

    return nFrames;
}

size_t CNukedMT32Synth::Render(
    float* pOutBuffer,
    size_t nFrames
)
{
    if (!pOutBuffer)
        return nFrames;

    m_Lock.Acquire();

    if (!m_bInitialized || !m_pResamplerModel)
    {
        std::memset(
            pOutBuffer,
            0,
            nFrames * 2 * sizeof(*pOutBuffer)
        );
    }
    else
    {
        m_pResamplerModel->getOutputSamples(
            pOutBuffer,
            static_cast<unsigned int>(nFrames)
        );

        if (m_bReversedStereo)
        {
            for (size_t i = 0; i < nFrames; ++i)
            {
                const float nLeft =
                    pOutBuffer[i * 2];

                pOutBuffer[i * 2] =
                    pOutBuffer[i * 2 + 1];

                pOutBuffer[i * 2 + 1] =
                    nLeft;
            }
        }
    }

    m_Lock.Release();

    return nFrames;
}

void CNukedMT32Synth::ReportStatus() const
{
    if (!m_pUI)
        return;

    const char* pVersion = "unknown";

    if (m_pControlROMImage)
    {
        const MT32Emu::ROMInfo* const pROMInfo =
            m_pControlROMImage->getROMInfo();

        if (pROMInfo && pROMInfo->shortName)
        {
            const char* const pShortName =
                pROMInfo->shortName;

            if (strstr(pShortName, "ctrl_mt32_1_04"))
                pVersion = "1.04";
            else if (strstr(pShortName, "ctrl_mt32_1_05"))
                pVersion = "1.05";
            else if (strstr(pShortName, "ctrl_mt32_1_06"))
                pVersion = "1.06";
            else if (strstr(pShortName, "ctrl_mt32_1_07"))
                pVersion = "1.07";
            else if (strstr(pShortName, "ctrl_mt32_2_04"))
                pVersion = "2.04";
            else if (strstr(pShortName, "ctrl_mt32_2_06"))
                pVersion = "2.06";
            else if (strstr(pShortName, "ctrl_mt32_2_07"))
                pVersion = "2.07";
            else if (m_CurrentROMSet == TMT32ROMSet::MT32Old)
                pVersion = "1.0x";
            else if (m_CurrentROMSet == TMT32ROMSet::MT32New)
                pVersion = "2.0x";
        }
    }

    char Message[32];

    snprintf(
        Message,
        sizeof(Message),
        "Nuked-MT32 %s",
        pVersion
    );

    m_pUI->ShowSystemMessage(Message);
}

void CNukedMT32Synth::UpdateLCD(
    CLCD& LCD,
    unsigned int nTicks
)
{
    const u8 nHeight = LCD.Height();

    const u8 nStatusRow =
        LCD.GetType() == CLCD::TType::Character
            ? nHeight - 1
            : nHeight / 16 - 1;

    const u8 nBarHeight =
        LCD.GetType() == CLCD::TType::Character
            ? nHeight - 1
            : nHeight - 16;

    float ChannelLevels[16];
    float ChannelPeaks[16];

    const u16 nPercussionMask =
        static_cast<u16>(
            1 << m_MIDIChannelPartMap[MT32PartCount - 1]
        );

    m_MIDIMonitor.GetChannelLevels(
        nTicks,
        ChannelLevels,
        ChannelPeaks,
        nPercussionMask
    );

    float PartLevels[MT32PartCount];
    float PartPeaks[MT32PartCount];

    for (u8 nPart = 0; nPart < MT32PartCount; ++nPart)
    {
        const u8 nChannel =
            m_MIDIChannelPartMap[nPart];

        PartLevels[nPart] = ChannelLevels[nChannel];
        PartPeaks[nPart] = ChannelPeaks[nChannel];
    }

    CUserInterface::DrawChannelLevels(
        LCD,
        nBarHeight,
        PartLevels,
        PartPeaks,
        MT32PartCount,
        false
    );

    if (m_bInitialized && m_pMT32->lcd_is_on())
    {
        const u8* const pText = m_pMT32->lcd_text();

        for (size_t i = 0; i < LCDTextLength; ++i)
        {
            const u8 nCharacter = pText[i];

            if (nCharacter == 0x01)
            {
                m_LCDText[i] = '\xFF';
            }
            else if (
                nCharacter >= 0x20 &&
                nCharacter < 0x7F
            )
            {
                m_LCDText[i] =
                    static_cast<char>(nCharacter);
            }
            else
            {
                m_LCDText[i] = ' ';
            }
        }

        m_LCDText[LCDTextLength] = '\0';
    }

    LCD.Print(
        m_LCDText,
        0,
        nStatusRow,
        true,
        false
    );
}

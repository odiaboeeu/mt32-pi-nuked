//
// sc55synth.cpp
//
// Experimental Nuked-SC55 backend for mt32-pi
//

#include <fatfs/ff.h>
#include <cstring>
#include <stdio.h>

#include <circle/logger.h>
#include <circle/sched/task.h>
#include <circle/sched/scheduler.h>

#include "lcd/lcd.h"
#include "lcd/ui.h"
#include "synth/sc55synth.h"
#include "utility.h"

LOGMODULE("sc55synth");

extern "C"
{
        int SC55_HeadlessLoadMk2RomSetFromMemory(
                const unsigned char* rom1_data, unsigned int rom1_size,
                const unsigned char* rom2_data, unsigned int rom2_size,
                const unsigned char* waverom1_data, unsigned int waverom1_size,
                const unsigned char* waverom2_data, unsigned int waverom2_size,
                const unsigned char* rom_sm_data, unsigned int rom_sm_size);

        int SC55_HeadlessLoadMk1RomSetFromMemory(
                const unsigned char* rom1_data, unsigned int rom1_size,
                const unsigned char* rom2_data, unsigned int rom2_size,
                const unsigned char* waverom1_data, unsigned int waverom1_size,
                const unsigned char* waverom2_data, unsigned int waverom2_size,
                const unsigned char* waverom3_data, unsigned int waverom3_size);

        int SC55_HeadlessOpenAudio(int pageSize, int pageNum);
        void SC55_HeadlessCloseAudio(void);
        void SC55_HeadlessInit(void);
        void SC55_HeadlessReset(void);
        void SC55_HeadlessPostMIDIByte(unsigned char data);
        void SC55_HeadlessRunStep(void);
        void SC55_HeadlessRunSteps(unsigned int count);
        int SC55_HeadlessPopSample(short* left, short* right);
}


namespace
{
        constexpr size_t SC55RingFrames = 131072;
        constexpr size_t SC55PrebufferFrames = 32768;

        short g_SC55RingLeft[SC55RingFrames];
        short g_SC55RingRight[SC55RingFrames];

        static_assert(
            (SC55RingFrames & (SC55RingFrames - 1)) == 0,
            "SC55RingFrames must be a power of two"
        );

        constexpr size_t SC55RingMask = SC55RingFrames - 1;

        // Single-producer/single-consumer counters.
        // Core 3 writes only g_SC55RingWrite.
        // Core 2 writes only g_SC55RingRead.
        alignas(64) volatile size_t g_SC55RingRead = 0;
        alignas(64) volatile size_t g_SC55RingWrite = 0;

        size_t SC55RingCount()
        {
                const size_t nWrite = g_SC55RingWrite;
                const size_t nRead = g_SC55RingRead;
                return nWrite - nRead;
        }

        unsigned g_SC55ResampleAccumulator = 0;
        short g_SC55LastLeft = 0;
        short g_SC55LastRight = 0;
        size_t g_SC55Underruns = 0;
        size_t g_SC55ProducedSamples = 0;
        size_t g_SC55ConsumedSamples = 0;

        bool SC55RingPush(short left, short right)
        {
                const size_t nWrite = g_SC55RingWrite;
                const size_t nRead = g_SC55RingRead;

                if (nWrite - nRead >= SC55RingFrames)
                        return false;

                const size_t nIndex = nWrite & SC55RingMask;

                g_SC55RingLeft[nIndex] = left;
                g_SC55RingRight[nIndex] = right;

                // Publish the completed frame to the consumer.
                g_SC55RingWrite = nWrite + 1;
                ++g_SC55ProducedSamples;
                return true;
        }

        bool SC55RingPop(short& left, short& right)
        {
                const size_t nRead = g_SC55RingRead;
                const size_t nWrite = g_SC55RingWrite;

                if (nRead == nWrite)
                        return false;

                const size_t nIndex = nRead & SC55RingMask;

                left = g_SC55RingLeft[nIndex];
                right = g_SC55RingRight[nIndex];

                // Release the consumed frame to the producer.
                g_SC55RingRead = nRead + 1;
                ++g_SC55ConsumedSamples;
                return true;
        }

        void SC55RingClear()
        {
                g_SC55RingRead = 0;
                g_SC55RingWrite = 0;
                g_SC55ResampleAccumulator = 0;
                g_SC55LastLeft = 0;
                g_SC55LastRight = 0;
                        g_SC55Underruns = 0;
}
}


class CSC55ProducerTask : public CTask
{
public:
        explicit CSC55ProducerTask(CSC55Synth* pSynth)
                : CTask(),
                  m_pSynth(pSynth)
        {
        }

        virtual void Run() override
        {
                while (m_pSynth && m_pSynth->IsProducerRunning())
                {
                        m_pSynth->Pump(8192);

                        // Cooperative scheduler: yield so MIDI/UI/audio tasks can run.
                        CScheduler::Get()->Yield();
                }
        }

private:
        CSC55Synth* m_pSynth;
};

// Kept intentionally so the linker can be forced to retain the experimental core.
extern "C" void SC55_LinkProbe(void)
{
        volatile auto p0 = (void*)SC55_HeadlessLoadMk2RomSetFromMemory;
        volatile auto p0mk1 = (void*)SC55_HeadlessLoadMk1RomSetFromMemory;
        volatile auto p1 = (void*)SC55_HeadlessOpenAudio;
        volatile auto p2 = (void*)SC55_HeadlessCloseAudio;
        volatile auto p3 = (void*)SC55_HeadlessInit;
        volatile auto p4 = (void*)SC55_HeadlessReset;
        volatile auto p5 = (void*)SC55_HeadlessPostMIDIByte;
        volatile auto p6 = (void*)SC55_HeadlessRunStep;
        volatile auto p7 = (void*)SC55_HeadlessPopSample;

        (void)p0;
        (void)p0mk1;
        (void)p1;
        (void)p2;
        (void)p3;
        (void)p4;
        (void)p5;
        (void)p6;
        (void)p7;
}

CSC55Synth::CSC55Synth(
        unsigned nSampleRate,
        TSC55Model Model,
        bool bDebug)
        : CSynthBase(nSampleRate),
          m_pProducerTask(nullptr),
          m_bProducerRunning(false),
          m_bInitialized(false),
          m_Model(Model),
          m_nNativeSampleRate(
              Model == TSC55Model::MK1 ? 64000 : 66207
          ),
          m_bDebug(bDebug),
          m_nVolume(100)
{
}

CSC55Synth::~CSC55Synth()
{
        StopProducer();

        if (m_bInitialized)
                SC55_HeadlessCloseAudio();
}

void CSC55Synth::FreeROMBuffer(u8*& pData)
{
        delete[] pData;
        pData = nullptr;
}

bool CSC55Synth::LoadROMFile(const char* pPath, u8*& pOutData, unsigned int& nOutSize)
{
        pOutData = nullptr;
        nOutSize = 0;

        FIL File;
        FRESULT Result = f_open(&File, pPath, FA_READ);

        if (Result != FR_OK)
        {
                LOGERR("Failed to open ROM: %s", pPath);
                return false;
        }

        FSIZE_t nSize = f_size(&File);
        if (nSize == 0)
        {
                LOGERR("Empty ROM: %s", pPath);
                f_close(&File);
                return false;
        }

        pOutData = new u8[nSize];
        if (!pOutData)
        {
            LOGERR("Failed to allocate ROM buffer: %s", pPath);
            f_close(&File);
            return false;
        }

        UINT nRead = 0;
        Result = f_read(&File, pOutData, nSize, &nRead);
        f_close(&File);

        if (Result != FR_OK || nRead != nSize)
        {
                LOGERR("Failed to read ROM: %s", pPath);
                FreeROMBuffer(pOutData);
                return false;
        }

        nOutSize = static_cast<unsigned int>(nSize);
        LOGNOTE("Loaded ROM %s (%u bytes)", pPath, nOutSize);
        return true;
}

static bool ValidateSC55ROMSize(
        const char* pPath,
        unsigned int nActualSize,
        unsigned int nExpectedSize
)
{
        if (nActualSize == nExpectedSize)
                return true;

        LOGERR(
                "Invalid ROM size for %s: %u bytes, expected %u",
                pPath,
                nActualSize,
                nExpectedSize
        );

        return false;
}

bool CSC55Synth::Initialize()
{
        u8* pROM1 = nullptr;
        u8* pROM2 = nullptr;
        u8* pWaveROM1 = nullptr;
        u8* pWaveROM2 = nullptr;
        u8* pWaveROM3 = nullptr;
        u8* pROMSM = nullptr;

        unsigned int nROM1Size = 0;
        unsigned int nROM2Size = 0;
        unsigned int nWaveROM1Size = 0;
        unsigned int nWaveROM2Size = 0;
        unsigned int nWaveROM3Size = 0;
        unsigned int nROMSMSize = 0;

        bool bOK = false;

        if (m_Model == TSC55Model::MK1)
        {
                bOK =
                        LoadROMFile(
                                "roms/sc55/mk1/rom1.bin",
                                pROM1,
                                nROM1Size
                        ) &&
                        LoadROMFile(
                                "roms/sc55/mk1/rom2.bin",
                                pROM2,
                                nROM2Size
                        ) &&
                        LoadROMFile(
                                "roms/sc55/mk1/waverom1.bin",
                                pWaveROM1,
                                nWaveROM1Size
                        ) &&
                        LoadROMFile(
                                "roms/sc55/mk1/waverom2.bin",
                                pWaveROM2,
                                nWaveROM2Size
                        ) &&
                        LoadROMFile(
                                "roms/sc55/mk1/waverom3.bin",
                                pWaveROM3,
                                nWaveROM3Size
                        );

                if (bOK)
                {
                        bOK =
                                ValidateSC55ROMSize(
                                        "roms/sc55/mk1/rom1.bin",
                                        nROM1Size,
                                        32U * 1024U
                                ) &&
                                ValidateSC55ROMSize(
                                        "roms/sc55/mk1/rom2.bin",
                                        nROM2Size,
                                        256U * 1024U
                                ) &&
                                ValidateSC55ROMSize(
                                        "roms/sc55/mk1/waverom1.bin",
                                        nWaveROM1Size,
                                        1024U * 1024U
                                ) &&
                                ValidateSC55ROMSize(
                                        "roms/sc55/mk1/waverom2.bin",
                                        nWaveROM2Size,
                                        1024U * 1024U
                                ) &&
                                ValidateSC55ROMSize(
                                        "roms/sc55/mk1/waverom3.bin",
                                        nWaveROM3Size,
                                        1024U * 1024U
                                );
                }

                if (bOK)
                {
                        bOK = SC55_HeadlessLoadMk1RomSetFromMemory(
                                pROM1,
                                nROM1Size,
                                pROM2,
                                nROM2Size,
                                pWaveROM1,
                                nWaveROM1Size,
                                pWaveROM2,
                                nWaveROM2Size,
                                pWaveROM3,
                                nWaveROM3Size
                        );

                        if (!bOK)
                        {
                                LOGERR(
                                        "SC55_HeadlessLoadMk1RomSetFromMemory failed"
                                );
                        }
                }
        }
        else
        {
                bOK =
                        LoadROMFile(
                                "roms/sc55/mk2/rom1.bin",
                                pROM1,
                                nROM1Size
                        ) &&
                        LoadROMFile(
                                "roms/sc55/mk2/rom2.bin",
                                pROM2,
                                nROM2Size
                        ) &&
                        LoadROMFile(
                                "roms/sc55/mk2/waverom1.bin",
                                pWaveROM1,
                                nWaveROM1Size
                        ) &&
                        LoadROMFile(
                                "roms/sc55/mk2/waverom2.bin",
                                pWaveROM2,
                                nWaveROM2Size
                        ) &&
                        LoadROMFile(
                                "roms/sc55/mk2/rom_sm.bin",
                                pROMSM,
                                nROMSMSize
                        );

                if (bOK)
                {
                        bOK =
                                ValidateSC55ROMSize(
                                        "roms/sc55/mk2/rom1.bin",
                                        nROM1Size,
                                        32U * 1024U
                                ) &&
                                ValidateSC55ROMSize(
                                        "roms/sc55/mk2/rom2.bin",
                                        nROM2Size,
                                        512U * 1024U
                                ) &&
                                ValidateSC55ROMSize(
                                        "roms/sc55/mk2/waverom1.bin",
                                        nWaveROM1Size,
                                        2U * 1024U * 1024U
                                ) &&
                                ValidateSC55ROMSize(
                                        "roms/sc55/mk2/waverom2.bin",
                                        nWaveROM2Size,
                                        1024U * 1024U
                                ) &&
                                ValidateSC55ROMSize(
                                        "roms/sc55/mk2/rom_sm.bin",
                                        nROMSMSize,
                                        4U * 1024U
                                );
                }

                if (bOK)
                {
                        bOK = SC55_HeadlessLoadMk2RomSetFromMemory(
                                pROM1,
                                nROM1Size,
                                pROM2,
                                nROM2Size,
                                pWaveROM1,
                                nWaveROM1Size,
                                pWaveROM2,
                                nWaveROM2Size,
                                pROMSM,
                                nROMSMSize
                        );

                        if (!bOK)
                        {
                                LOGERR(
                                        "SC55_HeadlessLoadMk2RomSetFromMemory failed"
                                );
                        }
                }
        }

        FreeROMBuffer(pROM1);
        FreeROMBuffer(pROM2);
        FreeROMBuffer(pWaveROM1);
        FreeROMBuffer(pWaveROM2);
        FreeROMBuffer(pWaveROM3);
        FreeROMBuffer(pROMSM);

        if (!bOK)
                return false;

        if (!SC55_HeadlessOpenAudio(512, 64))
        {
                LOGERR("SC55_HeadlessOpenAudio failed");
                return false;
        }

        SC55_HeadlessInit();
        SC55RingClear();

        m_bInitialized = true;

        // Pre-fill the native sample ring outside the audio render path.
        Pump(200000);

        m_bProducerRunning = true;

        // Producer runs on the dedicated physical Core 3.
        m_pProducerTask = nullptr;

        if (m_Model == TSC55Model::MK1)
                LOGNOTE("Experimental Nuked-SC55mkI initialized");
        else
                LOGNOTE("Experimental Nuked-SC55mkII initialized");

        return true;
}

void CSC55Synth::HandleMIDIShortMessage(u32 nMessage)
{
        if (!m_bInitialized)
                return;

        const u8 nStatus = nMessage & 0xFF;
        const u8 nData1 = (nMessage >> 8) & 0xFF;
        const u8 nData2 = (nMessage >> 16) & 0xFF;

        m_Lock.Acquire();

        SC55_HeadlessPostMIDIByte(nStatus);

        if ((nStatus & 0xF0) != 0xC0 && (nStatus & 0xF0) != 0xD0 && nStatus < 0xF8)
        {
                SC55_HeadlessPostMIDIByte(nData1);
                SC55_HeadlessPostMIDIByte(nData2);
        }
        else if (nStatus < 0xF8)
        {
                SC55_HeadlessPostMIDIByte(nData1);
        }

        m_Lock.Release();

        CSynthBase::HandleMIDIShortMessage(nMessage);
}

void CSC55Synth::HandleMIDISysExMessage(const u8* pData, size_t nSize)
{
        if (!m_bInitialized || !pData || nSize == 0)
                return;

        m_Lock.Acquire();

        for (size_t i = 0; i < nSize; ++i)
                SC55_HeadlessPostMIDIByte(pData[i]);

        m_Lock.Release();
}

void CSC55Synth::AllSoundOff()
{
        // CC 120 / All Sound Off on all channels.
        for (u8 ch = 0; ch < 16; ++ch)
        {
                SC55_HeadlessPostMIDIByte(0xB0 | ch);
                SC55_HeadlessPostMIDIByte(120);
                SC55_HeadlessPostMIDIByte(0);
        }

        CSynthBase::AllSoundOff();
}

void CSC55Synth::SetMasterVolume(u8 nVolume)
{
        m_nVolume = nVolume;
}



void CSC55Synth::StopProducer()
{
        m_bProducerRunning = false;

        // Cooperative task will exit on its next yield/run.
        // We intentionally do not delete m_pProducerTask here in this POC,
        // because Circle tasks manage their lifetime after Run() returns.
        m_pProducerTask = nullptr;
}

void CSC55Synth::Pump(size_t nMaxSteps)
{
        if (!m_bInitialized)
                return;

        constexpr size_t RunBatchSteps = 64;
        constexpr size_t SampleBatchFrames = 512;

        short BatchLeft[SampleBatchFrames];
        short BatchRight[SampleBatchFrames];

        size_t nStepsRemaining = nMaxSteps;

        while (nStepsRemaining > 0 &&
               SC55RingCount() < SC55PrebufferFrames)
        {
                const size_t nRunSteps =
                    nStepsRemaining < RunBatchSteps
                        ? nStepsRemaining
                        : RunBatchSteps;

                SC55_HeadlessRunSteps(
                    static_cast<unsigned int>(nRunSteps)
                );

                nStepsRemaining -= nRunSteps;

                for (;;)
                {
                        size_t nBatchCount = 0;
                        short left = 0;
                        short right = 0;

                        while (nBatchCount < SampleBatchFrames &&
                               SC55_HeadlessPopSample(&left, &right))
                        {
                                BatchLeft[nBatchCount] = left;
                                BatchRight[nBatchCount] = right;
                                ++nBatchCount;
                        }

                        if (nBatchCount == 0)
                                break;

                        for (size_t i = 0; i < nBatchCount; ++i)
                        {
                                if (SC55RingCount() >= SC55RingFrames)
                                        return;

                                SC55RingPush(
                                    BatchLeft[i],
                                    BatchRight[i]
                                );
                        }

                        if (nBatchCount < SampleBatchFrames ||
                            SC55RingCount() >= SC55PrebufferFrames)
                        {
                                break;
                        }
                }
        }
}




size_t CSC55Synth::Render(s16* pOutBuffer, size_t nFrames)
{
        if (!m_bInitialized)
        {
                memset(pOutBuffer, 0, nFrames * 2 * sizeof(s16));
                return nFrames;
        }

        m_Lock.Acquire();

        // Experimental block scheduler:
        // advance the emulated SC-55 core continuously for this audio chunk,
        // then consume the generated native samples below.
        constexpr unsigned SC55StepsPerOutputFrame = 128;
        for (size_t step = 0; step < nFrames * SC55StepsPerOutputFrame; ++step)
                SC55_HeadlessRunStep();

        static unsigned s_nAccumulator = 0;
        static short s_LastLeft = 0;
        static short s_LastRight = 0;

        for (size_t i = 0; i < nFrames; ++i)
        {
                s_nAccumulator += m_nNativeSampleRate;

                bool bGotSample = false;

                while (s_nAccumulator >= m_nSampleRate)
                {
                        short left = s_LastLeft;
                        short right = s_LastRight;

                        if (!SC55_HeadlessPopSample(&left, &right))
                        {
                                left = s_LastLeft;
                                right = s_LastRight;
                        }

                        s_LastLeft = left;
                        s_LastRight = right;
                        bGotSample = true;

                        s_nAccumulator -= m_nSampleRate;
                }

                (void)bGotSample;

                pOutBuffer[i * 2 + 0] = static_cast<s16>((s_LastLeft * m_nVolume) / 100);
                pOutBuffer[i * 2 + 1] = static_cast<s16>((s_LastRight * m_nVolume) / 100);
        }

        m_Lock.Release();

        return nFrames;
}

size_t CSC55Synth::Render(float* pOutBuffer, size_t nFrames)
{
        if (!m_bInitialized)
        {
                memset(pOutBuffer, 0, nFrames * 2 * sizeof(float));
                return nFrames;
        }

        // Experimental underrun protection.
        // Start consuming only after the native ring has a useful prebuffer.
        // Stop consuming again if it drops too low.
        constexpr size_t StartThreshold = 4096;
        constexpr size_t StopThreshold = 1024;

        static bool s_bOutputEnabled = false;

        if (!s_bOutputEnabled)
        {
                if (SC55RingCount() >= StartThreshold)
                {
                        s_bOutputEnabled = true;
                }
                else
                {
                        memset(pOutBuffer, 0, nFrames * 2 * sizeof(float));
                        return nFrames;
                }
        }
        else if (SC55RingCount() < StopThreshold)
        {
                s_bOutputEnabled = false;
                ++g_SC55Underruns;
                memset(pOutBuffer, 0, nFrames * 2 * sizeof(float));
                return nFrames;
        }

        for (size_t i = 0; i < nFrames; ++i)
        {
                g_SC55ResampleAccumulator += m_nNativeSampleRate;

                while (g_SC55ResampleAccumulator >= m_nSampleRate)
                {
                        short left = g_SC55LastLeft;
                        short right = g_SC55LastRight;

                        if (SC55RingPop(left, right))
                        {
                                g_SC55LastLeft = left;
                                g_SC55LastRight = right;
                        }
                        else
                        {
                                s_bOutputEnabled = false;
                                ++g_SC55Underruns;
                                break;
                        }

                        g_SC55ResampleAccumulator -= m_nSampleRate;
                }

                pOutBuffer[i * 2 + 0] = (g_SC55LastLeft / 32768.0f) * (m_nVolume / 100.0f);
                pOutBuffer[i * 2 + 1] = (g_SC55LastRight / 32768.0f) * (m_nVolume / 100.0f);
        }

        return nFrames;
}







void CSC55Synth::ReportStatus() const
{
        if (!m_pUI)
                return;

        if (m_Model == TSC55Model::MK1)
                m_pUI->ShowSystemMessage("Nuked-SC55mkI");
        else
                m_pUI->ShowSystemMessage("Nuked-SC55mkII");
}

void CSC55Synth::UpdateLCD(CLCD& LCD, unsigned int nTicks)
{
        if (!m_bDebug)
        {
                const u8 nBarHeight = LCD.Height();
                float ChannelLevels[16];
                float PeakLevels[16];

                constexpr u16 PercussionMask = 1 << 9;

                m_MIDIMonitor.GetChannelLevels(
                    nTicks,
                    ChannelLevels,
                    PeakLevels,
                    PercussionMask
                );

                CUserInterface::DrawChannelLevels(
                    LCD,
                    nBarHeight,
                    ChannelLevels,
                    PeakLevels,
                    10,
                    true
                );

                return;
        }

        static unsigned s_nLastUpdateTicks = 0;
        static char s_Line1[32] = "SC55";
        static char s_Line2[32] = "";

        static size_t s_nLastProduced = 0;
        static size_t s_nLastConsumed = 0;

        if (nTicks - s_nLastUpdateTicks >=
            static_cast<unsigned>(Utility::MillisToTicks(1000)))
        {
                const size_t nProducedPerSec =
                    g_SC55ProducedSamples - s_nLastProduced;

                const size_t nConsumedPerSec =
                    g_SC55ConsumedSamples - s_nLastConsumed;

                snprintf(
                    s_Line1,
                    sizeof(s_Line1),
                    "R:%lu U:%lu",
                    static_cast<unsigned long>(SC55RingCount()),
                    static_cast<unsigned long>(g_SC55Underruns)
                );

                snprintf(
                    s_Line2,
                    sizeof(s_Line2),
                    "P:%lu C:%lu",
                    static_cast<unsigned long>(nProducedPerSec),
                    static_cast<unsigned long>(nConsumedPerSec)
                );

                s_nLastProduced = g_SC55ProducedSamples;
                s_nLastConsumed = g_SC55ConsumedSamples;
                s_nLastUpdateTicks = nTicks;
        }

        LCD.Print(s_Line1, 0, 0, true, false);
        LCD.Print(s_Line2, 0, 1, true, false);
}



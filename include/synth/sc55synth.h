//
// sc55synth.h
//
// Experimental Nuked-SC55 backend for mt32-pi
//

#ifndef _sc55synth_h
#define _sc55synth_h

#include <circle/types.h>

#include "synth/sc55model.h"
#include "synth/synthbase.h"

class CSC55ProducerTask;

class CSC55Synth : public CSynthBase
{
public:
        explicit CSC55Synth(unsigned nSampleRate, TSC55Model Model, bool bDebug);
        virtual ~CSC55Synth() override;

        // CSynthBase
        virtual bool Initialize() override;
        void Pump(size_t nMaxSteps);
        void StopProducer();
        bool IsProducerRunning() const { return m_bProducerRunning; }
        virtual void HandleMIDIShortMessage(u32 nMessage) override;
        virtual void HandleMIDISysExMessage(const u8* pData, size_t nSize) override;
        virtual bool IsActive() override { return m_bInitialized; }
        virtual void AllSoundOff() override;
        virtual void SetMasterVolume(u8 nVolume) override;
        virtual size_t Render(s16* pOutBuffer, size_t nFrames) override;
        virtual size_t Render(float* pOutBuffer, size_t nFrames) override;
        virtual void ReportStatus() const override;
        virtual void UpdateLCD(CLCD& LCD, unsigned int nTicks) override;

private:
        bool LoadROMFile(const char* pPath, u8*& pOutData, unsigned int& nOutSize);
        void FreeROMBuffer(u8*& pData);
        void ProcessMkIIAnalogFilter(short& Left, short& Right);

        CSC55ProducerTask* m_pProducerTask;
        volatile bool m_bProducerRunning;

        bool m_bInitialized;
        TSC55Model m_Model;
        unsigned m_nNativeSampleRate;
        bool m_bDebug;
        u8 m_nVolume;

        // SC-55mkII DAC-to-point-B analog reconstruction state.
        // Three measured RC poles from the Roland output network are
        // evaluated at the native 66,207 Hz rate.
        float m_MkIIAnalogL1;
        float m_MkIIAnalogL2;
        float m_MkIIAnalogL3;
        float m_MkIIAnalogR1;
        float m_MkIIAnalogR2;
        float m_MkIIAnalogR3;
};

#endif

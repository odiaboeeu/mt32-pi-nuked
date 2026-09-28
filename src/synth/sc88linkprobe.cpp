#include "sc88_headless_core.h"

extern "C" void SC88_LinkProbe()
{
    volatile auto create =
        &sc88_headless_create;

    volatile auto destroy =
        &sc88_headless_destroy;

    volatile auto isValid =
        &sc88_headless_is_valid;

    volatile auto boot =
        &sc88_headless_boot;

    volatile auto playShortMessage =
        &sc88_headless_play_short_message;

    volatile auto playSysEx =
        &sc88_headless_play_sysex;

    volatile auto renderInt16 =
        &sc88_headless_render_int16;

    volatile auto sampleRate =
        &sc88_headless_sample_rate;

    volatile auto displayText =
        &sc88_headless_get_display_text;

    volatile auto midiBacklog =
        &sc88_headless_midi_backlog;

    (void) create;
    (void) destroy;
    (void) isValid;
    (void) boot;
    (void) playShortMessage;
    (void) playSysEx;
    (void) renderInt16;
    (void) sampleRate;
    (void) displayText;
    (void) midiBacklog;
}

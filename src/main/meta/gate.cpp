/*
 * Copyright (C) 2025 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2025 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-gate
 * Created on: 3 авг. 2021 г.
 *
 * lsp-plugins-gate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-gate is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-gate. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/gate.h>

#define LSP_PLUGINS_GATE_VERSION_MAJOR       1
#define LSP_PLUGINS_GATE_VERSION_MINOR       0
#define LSP_PLUGINS_GATE_VERSION_MICRO       31

#define LSP_PLUGINS_GATE_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_GATE_VERSION_MAJOR, \
        LSP_PLUGINS_GATE_VERSION_MINOR, \
        LSP_PLUGINS_GATE_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        static const int plugin_classes[]           = { C_GATE, -1 };
        static const int clap_features_mono[]       = { CF_AUDIO_EFFECT, CF_MONO, -1 };
        static const int clap_features_stereo[]     = { CF_AUDIO_EFFECT, CF_STEREO, -1 };

        static const port_item_t gate_sc_modes[] =
        {
            { "Peak",           "sidechain.peak"            },
            { "RMS",            "sidechain.rms"             },
            { "LPF",            "sidechain.lpf"             },
            { "SMA",            "sidechain.sma"             },
            { NULL, NULL }
        };

        static const port_item_t gate_sc_sources[] =
        {
            { "Middle",         "sidechain.middle"          },
            { "Side",           "sidechain.side"            },
            { "Left",           "sidechain.left"            },
            { "Right",          "sidechain.right"           },
            { "Min",            "sidechain.min"             },
            { "Max",            "sidechain.max"             },
            { NULL, NULL }
        };

        static const port_item_t gate_sc_split_sources[] =
        {
            { "Left/Right",     "sidechain.left_right"      },
            { "Right/Left",     "sidechain.right_left"      },
            { "Mid/Side",       "sidechain.mid_side"        },
            { "Side/Mid",       "sidechain.side_mid"        },
            { "Min",            "sidechain.min"             },
            { "Max",            "sidechain.max"             },
            { NULL, NULL }
        };

        static const port_item_t gate_sc_type[] =
        {
            { "Internal",   "sidechain.internal"        },
            { "Link",       "sidechain.link"            },
            { NULL, NULL }
        };

        static const port_item_t gate_extsc_type[] =
        {
            { "Internal",   "sidechain.internal"        },
            { "External",   "sidechain.external"        },
            { "Link",       "sidechain.link"            },
            { NULL, NULL }
        };

        static const port_item_t gate_filter_slope[] =
        {
            { "off",        "eq.slope.off"              },
            { "12 dB/oct",  "eq.slope.12dbo"            },
            { "24 dB/oct",  "eq.slope.24dbo"            },
            { "36 dB/oct",  "eq.slope.36dbo"            },
            { NULL, NULL }
        };

        #define GATE_PREMIX \
            SWITCH("showpmx", "Show pre-mix overlay", "Show premix bar", 0.0f), \
            AMP_GAIN10("in2lk", "Input to Link mix", "In to Link mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("lk2in", "Link to Input mix", "Link to In mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("lk2sc", "Link to Sidechain mix", "Link to SC mix", GAIN_AMP_M_INF_DB)

        #define GATE_SC_PREMIX \
            GATE_PREMIX, \
            AMP_GAIN10("in2sc", "Input to Sidechain mix", "In to SC mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("sc2in", "Sidechain to Input mix", "SC to In mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("sc2lk", "Sidechain to Link mix", "SC to Link mix", GAIN_AMP_M_INF_DB)

        #define GATE_COMMON     \
            BYPASS,             \
            IN_GAIN,            \
            OUT_GAIN,           \
            SWITCH("showmx", "Show mix overlay", "Show mix bar", 0.0f), \
            SWITCH("showsc", "Show sidechain overlay", "Show SC bar", 0.0f), \
            SWITCH("pause", "Pause graph analysis", "Pause", 0.0f), \
            TRIGGER("clear", "Clear graph analysis", "Clear")

        #define GATE_MS_COMMON  \
            GATE_COMMON,        \
            SWITCH("msl", "Mid/Side listen", "M/S listen", 0.0f)

        #define GATE_SPLIT_COMMON \
            SWITCH("ssplit", "Stereo split", "Stereo split", 0.0f), \
            COMBO("sscs", "Split sidechain source", "Split SC source", gate_metadata::SC_SPLIT_SOURCE_DFL, gate_sc_split_sources)

        #define GATE_LINK(id, label, alias) \
            SWITCH(id, label, alias, 0.0f)

        #define GATE_SHM_LINK_MONO \
            OPT_RETURN_MONO("link", "shml", "Side-chain shared memory link")

        #define GATE_SHM_LINK_STEREO \
            OPT_RETURN_STEREO("link", "shml_", "Side-chain shared memory link")

        #define GATE_MONO_CHANNEL(sc_type, sct_dfl) \
            COMBO("sci", "Sidechain input", "SC input", sct_dfl, sc_type), \
            COMBO("scm", "Sidechain mode", "SC mode", gate_metadata::SC_MODE_DFL, gate_sc_modes), \
            CONTROL("sla", "Sidechain lookahead", "SC look", U_MSEC, gate_metadata::LOOKAHEAD), \
            SWITCH("scl", "Sidechain listen", "SC listen", 0.0f), \
            LOG_CONTROL("scr", "Sidechain reactivity", "SC react", U_MSEC, gate_metadata::REACTIVITY), \
            AMP_GAIN100("scp", "Sidechain preamp", "SC preamp", GAIN_AMP_0_DB), \
            COMBO("shpm", "High-pass filter mode", "HPF mode", 0, gate_filter_slope),      \
            LOG_CONTROL("shpf", "High-pass filter frequency", "HPF freq", U_HZ, gate_metadata::HPF),   \
            COMBO("slpm", "Low-pass filter mode", "LPF mode", 0, gate_filter_slope),      \
            LOG_CONTROL("slpf", "Low-pass filter frequency", "LPF freq", U_HZ, gate_metadata::LPF)

        #define GATE_STEREO_CHANNEL(id, label, alias, sc_type, sct_dfl) \
            COMBO("sci" id, "Sidechain input" label, "SC input" alias, sct_dfl, sc_type), \
            COMBO("scm" id, "Sidechain mode" label, "SC mode" alias, gate_metadata::SC_MODE_DFL, gate_sc_modes), \
            CONTROL("sla" id, "Sidechain lookahead" label, "SC look" alias, U_MSEC, gate_metadata::LOOKAHEAD), \
            SWITCH("scl" id, "Sidechain listen" label, "SC listen" alias, 0.0f), \
            COMBO("scs" id, "Sidechain source" label, "SC source" alias, gate_metadata::SC_SOURCE_DFL, gate_sc_sources), \
            LOG_CONTROL("scr" id, "Sidechain reactivity" label, "SC react" alias, U_MSEC, gate_metadata::REACTIVITY), \
            AMP_GAIN100("scp" id, "Sidechain preamp" label, "SC preamp" alias, GAIN_AMP_0_DB), \
            COMBO("shpm" id, "High-pass filter mode" label, "HPF mode " alias, 0, gate_filter_slope),      \
            LOG_CONTROL("shpf" id, "High-pass filter frequency" label, "HPF freq" alias, U_HZ, gate_metadata::HPF),   \
            COMBO("slpm" id, "Low-pass filter mode" label, "LPF mode" alias, 0, gate_filter_slope),      \
            LOG_CONTROL("slpf" id, "Low-pass filter frequency" label, "LPF freq" alias, U_HZ, gate_metadata::LPF)

        #define GATE_CHANNEL(id, label, alias) \
            SWITCH("gh" id, "Hysteresis" label, "Hysteresis" alias, 0.0f), \
            LOG_CONTROL("gt" id, "Curve threshold" label, "Thresh" alias, U_GAIN_AMP, gate_metadata::THRESHOLD), \
            LOG_CONTROL("gz" id, "Curve zone size" label, "Zone" alias, U_GAIN_AMP, gate_metadata::ZONE), \
            LOG_CONTROL("ht" id, "Hysteresis threshold" label, "Hyst thresh" alias, U_GAIN_AMP, gate_metadata::H_THRESHOLD), \
            LOG_CONTROL("hz" id, "Hysteresis zone size" label, "Hyst zone" alias, U_GAIN_AMP, gate_metadata::ZONE), \
            LOG_CONTROL("at" id, "Attack" label, "Att time" alias, U_MSEC, gate_metadata::ATTACK_TIME), \
            LOG_CONTROL("rt" id, "Release" label, "Rel time" alias, U_MSEC, gate_metadata::RELEASE_TIME), \
            CONTROL("hold" id, "Hold time" label, "Hold time" alias, U_MSEC, gate_metadata::HOLD_TIME), \
            LOG_CONTROL("gr" id, "Reduction" label, "Reduction" alias, U_GAIN_AMP, gate_metadata::REDUCTION), \
            LOG_CONTROL("mk" id, "Makeup gain" label, "Makeup" alias, U_GAIN_AMP, gate_metadata::MAKEUP), \
            AMP_GAIN10("cdr" id, "Dry gain" label, "Dry" alias, GAIN_AMP_M_INF_DB),     \
            AMP_GAIN10("cwt" id, "Wet gain" label, "Wet" alias, GAIN_AMP_0_DB), \
            PERCENTS("cdw" id, "Dry/Wet balance" label, "Dry/Wet" alias, 100.0f, 0.1f), \
            METER_OUT_GAIN("gzs" id, "Zone start" label, GAIN_AMP_P_24_DB), \
            METER_OUT_GAIN("hts" id, "Hysteresis threshold start" label, GAIN_AMP_P_24_DB), \
            METER_OUT_GAIN("hzs" id, "Hysteresis zone start" label, GAIN_AMP_P_24_DB), \
            MESH("cg" id, "Curve graph" label, 2, gate_metadata::CURVE_MESH_SIZE), \
            MESH("hg" id, "Hysteresis graph" label, 2, gate_metadata::CURVE_MESH_SIZE)

        #define GATE_AUDIO_METER(id, label, alias) \
            SWITCH("slv" id, "Sidechain level visibility" label, "Show SC" alias, 1.0f), \
            SWITCH("elv" id, "Envelope level visibility" label, "Show Env" alias, 1.0f), \
            SWITCH("grv" id, "Gain reduction visibility" label, "Show Gain" alias, 1.0f), \
            SWITCH("ilv" id, "Input level visibility" label, "Show In" alias, 1.0f), \
            SWITCH("olv" id, "Output level visibility" label, "Show Out" alias, 1.0f), \
            MESH("scg" id, "Sidechain graph" label, 2, gate_metadata::TIME_MESH_SIZE), \
            MESH("evg" id, "Envelope graph" label, 2, gate_metadata::TIME_MESH_SIZE), \
            MESH("grg" id, "Gain reduciton graph" label, 2, gate_metadata::TIME_MESH_SIZE + 4), \
            MESH("icg" id, "Gate input" label, 2, gate_metadata::TIME_MESH_SIZE + 2), \
            MESH("ocg" id, "Gate output" label, 2, gate_metadata::TIME_MESH_SIZE), \
            METER_OUT_GAIN("slm" id, "Sidechain level meter" label, GAIN_AMP_P_24_DB), \
            METER_OUT_GAIN("clm" id, "Curve level meter" label, GAIN_AMP_P_24_DB), \
            METER_OUT_GAIN("elm" id, "Envelope level meter" label, GAIN_AMP_P_24_DB), \
            METER_GAIN("rlm" id, "Reduction level meter" label, GAIN_AMP_0_DB), \
            METER_GAIN("ilm" id, "Input level meter" label, GAIN_AMP_P_24_DB), \
            METER_GAIN("olm" id, "Output level meter" label, GAIN_AMP_P_24_DB)

        static const port_t gate_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            GATE_SHM_LINK_MONO,
            GATE_PREMIX,
            GATE_COMMON,
            GATE_MONO_CHANNEL(gate_sc_type, 0),
            GATE_CHANNEL("", "", ""),
            GATE_AUDIO_METER("", "", ""),

            PORTS_END
        };

        static const port_t gate_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            GATE_SHM_LINK_STEREO,
            GATE_PREMIX,
            GATE_COMMON,
            GATE_SPLIT_COMMON,
            GATE_STEREO_CHANNEL("", "", "", gate_sc_type, 0),
            GATE_CHANNEL("", "", ""),
            GATE_AUDIO_METER("_l", " Left", " L"),
            GATE_AUDIO_METER("_r", " Right", " R"),

            PORTS_END
        };

        static const port_t gate_lr_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            GATE_SHM_LINK_STEREO,
            GATE_PREMIX,
            GATE_COMMON,
            GATE_LINK("clink", "Left/Right controls link", "L/R link"),
            GATE_STEREO_CHANNEL("_l", " Left", " L", gate_sc_type, 0),
            GATE_STEREO_CHANNEL("_r", " Right", " R", gate_sc_type, 0),
            GATE_CHANNEL("_l", " Left", " L"),
            GATE_CHANNEL("_r", " Right", " R"),
            GATE_AUDIO_METER("_l", " Left", " L"),
            GATE_AUDIO_METER("_r", " Right", " R"),

            PORTS_END
        };

        static const port_t gate_ms_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            GATE_SHM_LINK_STEREO,
            GATE_PREMIX,
            GATE_MS_COMMON,
            GATE_LINK("clink", "Mid/Side controls link", "M/S link"),
            GATE_STEREO_CHANNEL("_m", " Mid", " M", gate_sc_type, 0),
            GATE_STEREO_CHANNEL("_s", " Side", " S", gate_sc_type, 0),
            GATE_CHANNEL("_m", " Mid", " M"),
            GATE_CHANNEL("_s", " Side", " S"),
            GATE_AUDIO_METER("_m", " Mid", " M"),
            GATE_AUDIO_METER("_s", " Side", " S"),

            PORTS_END
        };

        static const port_t sc_gate_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            PORTS_MONO_SIDECHAIN,
            GATE_SHM_LINK_MONO,
            GATE_SC_PREMIX,
            GATE_COMMON,
            GATE_MONO_CHANNEL(gate_extsc_type, 1),
            GATE_CHANNEL("", "", ""),
            GATE_AUDIO_METER("", "", ""),

            PORTS_END
        };

        static const port_t sc_gate_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            GATE_SHM_LINK_STEREO,
            GATE_SC_PREMIX,
            GATE_COMMON,
            GATE_SPLIT_COMMON,
            GATE_STEREO_CHANNEL("", "", "", gate_extsc_type, 1),
            GATE_CHANNEL("", "", ""),
            GATE_AUDIO_METER("_l", " Left", " L"),
            GATE_AUDIO_METER("_r", " Right", " R"),

            PORTS_END
        };

        static const port_t sc_gate_lr_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            GATE_SHM_LINK_STEREO,
            GATE_SC_PREMIX,
            GATE_COMMON,
            GATE_LINK("clink", "Left/Right controls link", "L/R link"),
            GATE_STEREO_CHANNEL("_l", " Left", " L", gate_extsc_type, 1),
            GATE_STEREO_CHANNEL("_r", " Right", " R", gate_extsc_type, 1),
            GATE_CHANNEL("_l", " Left", " L"),
            GATE_CHANNEL("_r", " Right", " R"),
            GATE_AUDIO_METER("_l", " Left", " L"),
            GATE_AUDIO_METER("_r", " Right", " R"),

            PORTS_END
        };

        static const port_t sc_gate_ms_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            GATE_SHM_LINK_STEREO,
            GATE_SC_PREMIX,
            GATE_MS_COMMON,
            GATE_LINK("clink", "Mid/Side controls link", "M/S link"),
            GATE_STEREO_CHANNEL("_m", " Mid", " M", gate_extsc_type, 1),
            GATE_STEREO_CHANNEL("_s", " Side", " S", gate_extsc_type, 1),
            GATE_CHANNEL("_m", " Mid", " M"),
            GATE_CHANNEL("_s", " Side", " S"),
            GATE_AUDIO_METER("_m", " Mid", " M"),
            GATE_AUDIO_METER("_s", " Side", " S"),

            PORTS_END
        };

        const meta::bundle_t gate_bundle =
        {
            "gate",
            "Gate",
            B_DYNAMICS,
            "p6otNrilF0U",
            "This plugin performs gating of input signal. Flexible sidechain-control\nconfiguration provided. Additional Hysteresis curve is available to provide\naccurate control of the fading of the signal. Also additional dry/wet control\nallows one to mix processed and unprocessed signal together."
        };

        // Gate
        const meta::plugin_t  gate_mono =
        {
            "Gate Mono",
            "Gate Mono",
            "Gate Mono",
            "G1M",
            &developers::v_sadovnikov,
            "gate_mono",
            {
                LSP_LV2_URI("gate_mono"),
                LSP_LV2UI_URI("gate_mono"),
                "ur0e",
                LSP_VST3_UID("g1m     ur0e"),
                LSP_VST3UI_UID("g1m     ur0e"),
                LSP_LADSPA_GATE_BASE + 0,
                LSP_LADSPA_URI("gate_mono"),
                LSP_CLAP_URI("gate_mono"),
                LSP_GST_UID("gate_mono"),
            },
            LSP_PLUGINS_GATE_VERSION,
            plugin_classes,
            clap_features_mono,
            E_INLINE_DISPLAY,
            gate_mono_ports,
            "dynamics/gate/single.xml",
            NULL,
            mono_plugin_port_groups,
            &gate_bundle
        };

        const meta::plugin_t  gate_stereo =
        {
            "Gate Stereo",
            "Gate Stereo",
            "Gate Stereo",
            "G1S",
            &developers::v_sadovnikov,
            "gate_stereo",
            {
                LSP_LV2_URI("gate_stereo"),
                LSP_LV2UI_URI("gate_stereo"),
                "wg4o",
                LSP_VST3_UID("g1s     wg4o"),
                LSP_VST3UI_UID("g1s     wg4o"),
                LSP_LADSPA_GATE_BASE + 1,
                LSP_LADSPA_URI("gate_stereo"),
                LSP_CLAP_URI("gate_stereo"),
                LSP_GST_UID("gate_stereo"),
            },
            LSP_PLUGINS_GATE_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY,
            gate_stereo_ports,
            "dynamics/gate/single.xml",
            NULL,
            stereo_plugin_port_groups,
            &gate_bundle
        };

        const meta::plugin_t  gate_lr =
        {
            "Gate LeftRight",
            "Gate LeftRight",
            "Gate L/R",
            "G1LR",
            &developers::v_sadovnikov,
            "gate_lr",
            {
                LSP_LV2_URI("gate_lr"),
                LSP_LV2UI_URI("gate_lr"),
                "icmw",
                LSP_VST3_UID("g1lr    icmw"),
                LSP_VST3UI_UID("g1lr    icmw"),
                LSP_LADSPA_GATE_BASE + 2,
                LSP_LADSPA_URI("gate_lr"),
                LSP_CLAP_URI("gate_lr"),
                LSP_GST_UID("gate_lr"),
            },
            LSP_PLUGINS_GATE_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY,
            gate_lr_ports,
            "dynamics/gate/single.xml",
            NULL,
            stereo_plugin_port_groups,
            &gate_bundle
        };

        const meta::plugin_t  gate_ms =
        {
            "Gate MidSide",
            "Gate MidSide",
            "Gate M/S",
            "G1MS",
            &developers::v_sadovnikov,
            "gate_ms",
            {
                LSP_LV2_URI("gate_ms"),
                LSP_LV2UI_URI("gate_ms"),
                "zci1",
                LSP_VST3_UID("g1ms    zci1"),
                LSP_VST3UI_UID("g1ms    zci1"),
                LSP_LADSPA_GATE_BASE + 3,
                LSP_LADSPA_URI("gate_ms"),
                LSP_CLAP_URI("gate_ms"),
                LSP_GST_UID("gate_ms"),
            },
            LSP_PLUGINS_GATE_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY,
            gate_ms_ports,
            "dynamics/gate/single.xml",
            NULL,
            stereo_plugin_port_groups,
            &gate_bundle
        };

        // Sidechain gate
        const meta::plugin_t  sc_gate_mono =
        {
            "Sidechain-Gate Mono",
            "Sidechain Gate Mono",
            "SC Gate Mono",
            "SCG1M",
            &developers::v_sadovnikov,
            "sc_gate_mono",
            {
                LSP_LV2_URI("sc_gate_mono"),
                LSP_LV2UI_URI("sc_gate_mono"),
                "nnz2",
                LSP_VST3_UID("scg1m   nnz2"),
                LSP_VST3UI_UID("scg1m   nnz2"),
                LSP_LADSPA_GATE_BASE + 4,
                LSP_LADSPA_URI("sc_gate_mono"),
                LSP_CLAP_URI("sc_gate_mono"),
                LSP_GST_UID("sc_gate_mono"),
            },
            LSP_PLUGINS_GATE_VERSION,
            plugin_classes,
            clap_features_mono,
            E_INLINE_DISPLAY,
            sc_gate_mono_ports,
            "dynamics/gate/single.xml",
            NULL,
            mono_plugin_sidechain_port_groups,
            &gate_bundle
        };

        const meta::plugin_t  sc_gate_stereo =
        {
            "Sidechain-Gate Stereo",
            "Sidechain Gate Stereo",
            "SC Gate Stereo",
            "SCG1S",
            &developers::v_sadovnikov,
            "sc_gate_stereo",
            {
                LSP_LV2_URI("sc_gate_stereo"),
                LSP_LV2UI_URI("sc_gate_stereo"),
                "fosg",
                LSP_VST3_UID("scg1s   fosg"),
                LSP_VST3UI_UID("scg1s   fosg"),
                LSP_LADSPA_GATE_BASE + 5,
                LSP_LADSPA_URI("sc_gate_stereo"),
                LSP_CLAP_URI("sc_gate_stereo"),
                LSP_GST_UID("sc_gate_stereo"),
            },
            LSP_PLUGINS_GATE_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY,
            sc_gate_stereo_ports,
            "dynamics/gate/single.xml",
            NULL,
            stereo_plugin_sidechain_port_groups,
            &gate_bundle
        };

        const meta::plugin_t  sc_gate_lr =
        {
            "Sidechain-Gate LeftRight",
            "Sidechain Gate LeftRight",
            "SC Gate L/R",
            "SCG1LR",
            &developers::v_sadovnikov,
            "sc_gate_lr",
            {
                LSP_LV2_URI("sc_gate_lr"),
                LSP_LV2UI_URI("sc_gate_lr"),
                "fmxo",
                LSP_VST3_UID("scg1lr  fmxo"),
                LSP_VST3UI_UID("scg1lr  fmxo"),
                LSP_LADSPA_GATE_BASE + 6,
                LSP_LADSPA_URI("sc_gate_lr"),
                LSP_CLAP_URI("sc_gate_lr"),
                LSP_GST_UID("sc_gate_lr"),
            },
            LSP_PLUGINS_GATE_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY,
            sc_gate_lr_ports,
            "dynamics/gate/single.xml",
            NULL,
            stereo_plugin_sidechain_port_groups,
            &gate_bundle
        };

        const meta::plugin_t  sc_gate_ms =
        {
            "Sidechain-Gate MidSide",
            "Sidechain Gate MidSide",
            "SC Gate M/S",
            "SCG1MS",
            &developers::v_sadovnikov,
            "sc_gate_ms",
            {
                LSP_LV2_URI("sc_gate_ms"),
                LSP_LV2UI_URI("sc_gate_ms"),
                "l6lc",
                LSP_VST3_UID("scg1ms  l6lc"),
                LSP_VST3UI_UID("scg1ms  l6lc"),
                LSP_LADSPA_GATE_BASE + 7,
                LSP_LADSPA_URI("sc_gate_ms"),
                LSP_CLAP_URI("sc_gate_ms"),
                LSP_GST_UID("sc_gate_ms"),
            },
            LSP_PLUGINS_GATE_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY,
            sc_gate_ms_ports,
            "dynamics/gate/single.xml",
            NULL,
            stereo_plugin_sidechain_port_groups,
            &gate_bundle
        };
    } /* namespace meta */
} /* namespace lsp */

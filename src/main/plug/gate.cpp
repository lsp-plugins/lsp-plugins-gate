/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Vladimir Sadovnikov <sadko4u@gmail.com>
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

#include <private/plugins/gate.h>
#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/shared/id_colors.h>

#define GATE_BUF_SIZE           0x1000
#define TRACE_PORT(p)           lsp_trace("  port id=%s", (p)->metadata()->id);

namespace lsp
{
    namespace plugins
    {
        //-------------------------------------------------------------------------
        // Plugin factory
        typedef struct plugin_settings_t
        {
            const meta::plugin_t   *metadata;
            bool                    sc;
            uint8_t                 mode;
        } plugin_settings_t;

        static const meta::plugin_t *plugins[] =
        {
            &meta::gate_mono,
            &meta::gate_stereo,
            &meta::gate_lr,
            &meta::gate_ms,
            &meta::sc_gate_mono,
            &meta::sc_gate_stereo,
            &meta::sc_gate_lr,
            &meta::sc_gate_ms
        };

        static const plugin_settings_t plugin_settings[] =
        {
            { &meta::gate_mono,       false, gate::GM_MONO          },
            { &meta::gate_stereo,     false, gate::GM_STEREO        },
            { &meta::gate_lr,         false, gate::GM_LR            },
            { &meta::gate_ms,         false, gate::GM_MS            },
            { &meta::sc_gate_mono,    true,  gate::GM_MONO          },
            { &meta::sc_gate_stereo,  true,  gate::GM_STEREO        },
            { &meta::sc_gate_lr,      true,  gate::GM_LR            },
            { &meta::sc_gate_ms,      true,  gate::GM_MS            },

            { NULL, 0, false }
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            for (const plugin_settings_t *s = plugin_settings; s->metadata != NULL; ++s)
                if (s->metadata == meta)
                    return new gate(s->metadata, s->sc, s->mode);
            return NULL;
        }

        static plug::Factory factory(plugin_factory, plugins, 8);

        //-------------------------------------------------------------------------
        gate::gate(const meta::plugin_t *metadata, bool sc, size_t mode): plug::Module(metadata)
        {
            nMode           = mode;
            bSidechain      = sc;
            vChannels       = NULL;
            vCurve          = NULL;
            vTime           = NULL;
            bPause          = false;
            bClear          = false;
            bMSListen       = false;
            fInGain         = 1.0f;
            bUISync         = true;

            pBypass         = NULL;
            pInGain         = NULL;
            pOutGain        = NULL;
            pPause          = NULL;
            pClear          = NULL;
            pMSListen       = NULL;

            pData           = NULL;
            pIDisplay       = NULL;
        }

        gate::~gate()
        {
        }

        void gate::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            plug::Module::init(wrapper, ports);
            size_t channels = (nMode == GM_MONO) ? 1 : 2;

            // Allocate temporary buffers
            size_t channel_size     = align_size(sizeof(channel_t) * channels, DEFAULT_ALIGN);
            size_t buf_size         = GATE_BUF_SIZE * sizeof(float);
            size_t curve_size       = (meta::gate_metadata::CURVE_MESH_SIZE) * sizeof(float);
            size_t history_size     = (meta::gate_metadata::TIME_MESH_SIZE) * sizeof(float);
            size_t allocate         = channel_size +
                                      buf_size * channels * 5 +
                                      curve_size +
                                      history_size;

            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, allocate);
            if (ptr == NULL)
                return;

            vChannels               = reinterpret_cast<channel_t *>(ptr);
            ptr                    += channel_size;
            vCurve                  = reinterpret_cast<float *>(ptr);
            ptr                    += curve_size;
            vTime                   = reinterpret_cast<float *>(ptr);
            ptr                    += history_size;

            // Initialize channels
            for (size_t i=0; i<channels; ++i)
            {
                // Construct the channel
                channel_t *c = &vChannels[i];
                c->sBypass.construct();
                c->sSC.construct();
                c->sSCEq.construct();
                c->sGate.construct();
                c->sLaDelay.construct();
                c->sInDelay.construct();
                c->sOutDelay.construct();
                c->sDryDelay.construct();
                for (size_t j=0; j<G_TOTAL; ++j)
                    c->sGraph[j].construct();

                // Init the channel
                if (!c->sSC.init(channels, meta::gate_metadata::REACTIVITY_MAX))
                    return;
                if (!c->sSCEq.init(2, 12))
                    return;
                c->sSCEq.set_mode(dspu::EQM_IIR);
                c->sSC.set_pre_equalizer(&c->sSCEq);

                c->vIn              = reinterpret_cast<float *>(ptr);
                ptr                += buf_size;
                c->vOut             = reinterpret_cast<float *>(ptr);
                ptr                += buf_size;
                c->vSc              = reinterpret_cast<float *>(ptr);
                ptr                += buf_size;
                c->vEnv             = reinterpret_cast<float *>(ptr);
                ptr                += buf_size;
                c->vGain            = reinterpret_cast<float *>(ptr);
                ptr                += buf_size;
                c->bScListen        = false;
                c->nSync            = S_ALL;
                c->nScType          = SCT_INTERNAL;
                c->fMakeup          = 1.0f;
                c->fDryGain         = 1.0f;
                c->fWetGain         = 0.0f;
                c->fDotIn           = 0.0f;
                c->fDotOut          = 0.0f;

                c->pIn              = NULL;
                c->pOut             = NULL;
                c->pSC              = NULL;

                for (size_t j=0; j<G_TOTAL; ++j)
                    c->pGraph[j]        = NULL;

                for (size_t j=0; j<M_TOTAL; ++j)
                    c->pMeter[j]        = NULL;

                c->pScType          = NULL;
                c->pScMode          = NULL;
                c->pScLookahead     = NULL;
                c->pScListen        = NULL;
                c->pScSource        = NULL;
                c->pScReactivity    = NULL;
                c->pScPreamp        = NULL;
                c->pScHpfMode       = NULL;
                c->pScHpfFreq       = NULL;
                c->pScLpfMode       = NULL;
                c->pScLpfFreq       = NULL;

                c->pHyst            = NULL;
                c->pThresh[0]       = NULL;
                c->pThresh[1]       = NULL;
                c->pZone[0]         = NULL;
                c->pZone[1]         = NULL;
                c->pAttack          = NULL;
                c->pRelease         = NULL;
                c->pReduction       = NULL;
                c->pMakeup          = NULL;

                c->pDryGain         = NULL;
                c->pWetGain         = NULL;
                c->pCurve[0]        = NULL;
                c->pCurve[1]        = NULL;
                c->pZoneStart[0]    = NULL;
                c->pZoneStart[1]    = NULL;
                c->pHystStart       = NULL;
            }

            lsp_assert(ptr < &pData[allocate]);

            // Bind ports
            size_t port_id              = 0;

            // Input ports
            lsp_trace("Binding input ports");
            for (size_t i=0; i<channels; ++i)
            {
                TRACE_PORT(ports[port_id]);
                vChannels[i].pIn        =   ports[port_id++];
            }

            // Input ports
            lsp_trace("Binding output ports");
            for (size_t i=0; i<channels; ++i)
            {
                TRACE_PORT(ports[port_id]);
                vChannels[i].pOut       =   ports[port_id++];
            }

            // Input ports
            if (bSidechain)
            {
                lsp_trace("Binding sidechain ports");
                for (size_t i=0; i<channels; ++i)
                {
                    TRACE_PORT(ports[port_id]);
                    vChannels[i].pSC        =   ports[port_id++];
                }
            }

            // Common ports
            lsp_trace("Binding common ports");
            TRACE_PORT(ports[port_id]);
            pBypass                 =   ports[port_id++];
            TRACE_PORT(ports[port_id]);
            pInGain                 =   ports[port_id++];
            TRACE_PORT(ports[port_id]);
            pOutGain                =   ports[port_id++];
            TRACE_PORT(ports[port_id]);
            pPause                  =   ports[port_id++];
            TRACE_PORT(ports[port_id]);
            pClear                  =   ports[port_id++];
            if (nMode == GM_MS)
            {
                TRACE_PORT(ports[port_id]);
                pMSListen               =   ports[port_id++];
            }

            // Sidechain ports
            lsp_trace("Binding sidechain ports");
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c = &vChannels[i];

                if ((i > 0) && (nMode == GM_STEREO))
                {
                    channel_t *sc       = &vChannels[0];
                    c->pScType          = sc->pScType;
                    c->pScSource        = sc->pScSource;
                    c->pScMode          = sc->pScMode;
                    c->pScLookahead     = sc->pScLookahead;
                    c->pScListen        = sc->pScListen;
                    c->pScReactivity    = sc->pScReactivity;
                    c->pScPreamp        = sc->pScPreamp;
                    c->pScHpfMode       = sc->pScHpfMode;
                    c->pScHpfFreq       = sc->pScHpfFreq;
                    c->pScLpfMode       = sc->pScLpfMode;
                    c->pScLpfFreq       = sc->pScLpfFreq;
                }
                else
                {
                    if (bSidechain)
                    {
                        TRACE_PORT(ports[port_id]);
                        c->pScType          =   ports[port_id++];
                    }
                    TRACE_PORT(ports[port_id]);
                    c->pScMode          =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pScLookahead     =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pScListen        =   ports[port_id++];
                    if (nMode != GM_MONO)
                    {
                        TRACE_PORT(ports[port_id]);
                        c->pScSource        =   ports[port_id++];
                    }
                    TRACE_PORT(ports[port_id]);
                    c->pScReactivity    =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pScPreamp        =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pScHpfMode       =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pScHpfFreq       =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pScLpfMode       =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pScLpfFreq       =   ports[port_id++];
                }
            }

            // Gate ports
            lsp_trace("Binding gate ports");
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c = &vChannels[i];

                if ((i > 0) && (nMode == GM_STEREO))
                {
                    channel_t *sc       = &vChannels[0];

                    c->pHyst            = sc->pHyst;
                    c->pThresh[0]       = sc->pThresh[0];
                    c->pThresh[1]       = sc->pThresh[1];
                    c->pZone[0]         = sc->pZone[0];
                    c->pZone[1]         = sc->pZone[1];
                    c->pAttack          = sc->pAttack;
                    c->pRelease         = sc->pRelease;
                    c->pReduction       = sc->pReduction;
                    c->pMakeup          = sc->pMakeup;

                    c->pDryGain         = sc->pDryGain;
                    c->pWetGain         = sc->pWetGain;
                    c->pZoneStart[0]    = sc->pZoneStart[0];
                    c->pZoneStart[1]    = sc->pZoneStart[1];
                    c->pHystStart       = sc->pHystStart;
                }
                else
                {
                    TRACE_PORT(ports[port_id]);
                    c->pHyst            =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pThresh[0]       =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pZone[0]        =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pThresh[1]       =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pZone[1]        =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pAttack          =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pRelease         =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pReduction       =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pMakeup          =   ports[port_id++];

                    TRACE_PORT(ports[port_id]);
                    c->pDryGain         =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pWetGain         =   ports[port_id++];

                    // Skip meters visibility controls
                    TRACE_PORT(ports[port_id]);
                    port_id++;
                    TRACE_PORT(ports[port_id]);
                    port_id++;
                    TRACE_PORT(ports[port_id]);
                    port_id++;

                    TRACE_PORT(ports[port_id]);
                    c->pZoneStart[0]    =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pHystStart       =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pZoneStart[1]    =   ports[port_id++];

                    TRACE_PORT(ports[port_id]);
                    c->pCurve[0]        =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pCurve[1]        =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pGraph[G_SC]     =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pGraph[G_ENV]    =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pGraph[G_GAIN]   =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pMeter[M_SC]     =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pMeter[M_CURVE]  =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pMeter[M_ENV]    =   ports[port_id++];
                    TRACE_PORT(ports[port_id]);
                    c->pMeter[M_GAIN]   =   ports[port_id++];
                }
            }

            // Bind history
            lsp_trace("Binding history ports");
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c = &vChannels[i];

                // Skip meters visibility controls
                TRACE_PORT(ports[port_id]);
                port_id++;
                TRACE_PORT(ports[port_id]);
                port_id++;

                // Bind ports
                TRACE_PORT(ports[port_id]);
                c->pGraph[G_IN]     =   ports[port_id++];
                TRACE_PORT(ports[port_id]);
                c->pGraph[G_OUT]    =   ports[port_id++];
                TRACE_PORT(ports[port_id]);
                c->pMeter[M_IN]     =   ports[port_id++];
                TRACE_PORT(ports[port_id]);
                c->pMeter[M_OUT]    =   ports[port_id++];
            }

            // Initialize curve (logarithmic) in range of -72 .. +24 db
            float delta = (meta::gate_metadata::CURVE_DB_MAX - meta::gate_metadata::CURVE_DB_MIN) / (meta::gate_metadata::CURVE_MESH_SIZE-1);
            for (size_t i=0; i<meta::gate_metadata::CURVE_MESH_SIZE; ++i)
                vCurve[i]   = dspu::db_to_gain(meta::gate_metadata::CURVE_DB_MIN + delta * i);

            // Initialize time points
            delta       = meta::gate_metadata::TIME_HISTORY_MAX / (meta::gate_metadata::TIME_MESH_SIZE - 1);
            for (size_t i=0; i<meta::gate_metadata::TIME_MESH_SIZE; ++i)
                vTime[i]    = meta::gate_metadata::TIME_HISTORY_MAX - i*delta;
        }

        void gate::destroy()
        {
            if (vChannels != NULL)
            {
                size_t channels = (nMode == GM_MONO) ? 1 : 2;
                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c = &vChannels[i];

                    c->sBypass.destroy();
                    c->sSC.destroy();
                    c->sSCEq.destroy();
                    c->sGate.destroy();
                    c->sLaDelay.destroy();
                    c->sInDelay.destroy();
                    c->sOutDelay.destroy();
                    c->sDryDelay.destroy();
                    for (size_t j=0; j<G_TOTAL; ++j)
                        c->sGraph[j].destroy();
                }

                vChannels = NULL;
            }

            if (pData != NULL)
            {
                free_aligned(pData);
                pData = NULL;
            }

            if (pIDisplay != NULL)
            {
                pIDisplay->destroy();
                pIDisplay   = NULL;
            }
        }

        void gate::update_sample_rate(long sr)
        {
            size_t samples_per_dot  = dspu::seconds_to_samples(sr, meta::gate_metadata::TIME_HISTORY_MAX / meta::gate_metadata::TIME_MESH_SIZE);
            size_t channels = (nMode == GM_MONO) ? 1 : 2;
            size_t max_delay    = dspu::millis_to_samples(fSampleRate, meta::gate_metadata::LOOKAHEAD_MAX);

            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c = &vChannels[i];
                c->sBypass.init(sr);
                c->sGate.set_sample_rate(sr);
                c->sSC.set_sample_rate(sr);
                c->sSCEq.set_sample_rate(sr);
                c->sLaDelay.init(max_delay);
                c->sInDelay.init(max_delay);
                c->sOutDelay.init(max_delay);
                c->sDryDelay.init(max_delay);

                for (size_t j=0; j<G_TOTAL; ++j)
                    c->sGraph[j].init(meta::gate_metadata::TIME_MESH_SIZE, samples_per_dot);

                c->sGraph[G_GAIN].fill(meta::gate_metadata::REDUCTION_DFL);
                c->sGraph[G_GAIN].set_method(dspu::MM_MINIMUM);
            }
        }

        void gate::update_settings()
        {
            dspu::filter_params_t fp;
            size_t channels = (nMode == GM_MONO) ? 1 : 2;
            bool bypass     = pBypass->value() >= 0.5f;

            // Global parameters
            bPause          = pPause->value() >= 0.5f;
            bClear          = pClear->value() >= 0.5f;
            bMSListen       = (pMSListen != NULL) ? pMSListen->value() >= 0.5f : false;
            fInGain         = pInGain->value();
            float out_gain  = pOutGain->value();
            size_t latency  = 0;

            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c    = &vChannels[i];

                // Update bypass settings
                c->sBypass.set_bypass(bypass);

                // Update sidechain settings
                c->nScType      = (c->pScType != NULL) ? c->pScType->value() : SCT_INTERNAL;
                c->bScListen    = c->pScListen->value() >= 0.5f;

                c->sSC.set_gain(c->pScPreamp->value());
                c->sSC.set_mode((c->pScMode != NULL) ? c->pScMode->value() : dspu::SCM_RMS);
                c->sSC.set_source((c->pScSource != NULL) ? c->pScSource->value() : dspu::SCS_MIDDLE);
                c->sSC.set_reactivity(c->pScReactivity->value());
                c->sSC.set_stereo_mode(((nMode == GM_MS) && (c->nScType != SCT_EXTERNAL)) ? dspu::SCSM_MIDSIDE : dspu::SCSM_STEREO);

                // Setup hi-pass filter for sidechain
                size_t hp_slope = c->pScHpfMode->value() * 2;
                fp.nType        = (hp_slope > 0) ? dspu::FLT_BT_BWC_HIPASS : dspu::FLT_NONE;
                fp.fFreq        = c->pScHpfFreq->value();
                fp.fFreq2       = fp.fFreq;
                fp.fGain        = 1.0f;
                fp.nSlope       = hp_slope;
                fp.fQuality     = 0.0f;
                c->sSCEq.set_params(0, &fp);

                // Setup low-pass filter for sidechain
                size_t lp_slope = c->pScLpfMode->value() * 2;
                fp.nType        = (lp_slope > 0) ? dspu::FLT_BT_BWC_LOPASS : dspu::FLT_NONE;
                fp.fFreq        = c->pScLpfFreq->value();
                fp.fFreq2       = fp.fFreq;
                fp.fGain        = 1.0f;
                fp.nSlope       = lp_slope;
                fp.fQuality     = 0.0f;
                c->sSCEq.set_params(1, &fp);

                // Update delay
                size_t delay    = dspu::millis_to_samples(fSampleRate, (c->pScLookahead != NULL) ? c->pScLookahead->value() : 0);
                c->sLaDelay.set_delay(delay);
                latency         = lsp_max(latency, delay);

                // Update Gate settings
                bool hyst       = (c->pHyst != NULL) ? (c->pHyst->value() >= 0.5f) : false;
                float thresh    = c->pThresh[0]->value();
                float hthresh   = (hyst) ? (thresh * c->pThresh[1]->value()) : thresh;
                float zone      = c->pZone[0]->value();
                float hzone     = (hyst) ? (c->pZone[1]->value()) : zone;
                float makeup    = c->pMakeup->value();

                c->sGate.set_threshold(thresh, hthresh);
                c->sGate.set_zone(zone, hzone);
                c->sGate.set_timings(c->pAttack->value(), c->pRelease->value());
                c->sGate.set_reduction(c->pReduction->value());

                if (c->pZoneStart[0] != NULL)
                    c->pZoneStart[0]->set_value(thresh * zone);
                if (c->pZoneStart[1] != NULL)
                    c->pZoneStart[1]->set_value(hthresh * hzone);
                if (c->pHystStart != NULL)
                    c->pHystStart->set_value(hthresh);

                // Check modification flag
                if (c->sGate.modified())
                {
                    c->sGate.update_settings();
                    c->nSync           |= S_ALL;
                }

                // Update gains
                c->fDryGain         = c->pDryGain->value() * out_gain;
                c->fWetGain         = c->pWetGain->value() * out_gain;
                if (c->fMakeup != makeup)
                {
                    c->fMakeup          = makeup;
                    c->nSync           |= S_ALL;
                }
            }

            // Tune compensation delays
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c    = &vChannels[i];
                c->sInDelay.set_delay(latency);
                c->sOutDelay.set_delay(latency - c->sLaDelay.get_delay());
                c->sDryDelay.set_delay(latency);
            }

            // Report latency
            set_latency(latency);
        }

        void gate::ui_activated()
        {
            size_t channels     = (nMode == GM_MONO) ? 1 : 2;
            for (size_t i=0; i<channels; ++i)
                vChannels[i].nSync     = S_ALL;
            bUISync             = true;
        }

        void gate::process(size_t samples)
        {
            size_t channels = (nMode == GM_MONO) ? 1 : 2;

            float *in_buf[2];   // Input buffer
            float *out_buf[2];  // Output buffer
            float *sc_buf[2];   // Sidechain source
            const float *in[2]; // Buffet to pass to sidechain

            // Prepare audio channels
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c        = &vChannels[i];

                // Initialize pointers
                in_buf[i]           = c->pIn->buffer<float>();
                out_buf[i]          = c->pOut->buffer<float>();
                sc_buf[i]           = (c->pSC != NULL) ? c->pSC->buffer<float>() : in_buf[i];
                c->fDotIn           = 0.0f;
                c->fDotOut          = 0.0f;
            }

            // Perform gating
            size_t left = samples;
            while (left > 0)
            {
                // Detemine number of samples to process
                size_t to_process = (left > GATE_BUF_SIZE) ? GATE_BUF_SIZE : left;

                // Prepare audio channels
                if (nMode == GM_MONO)
                    dsp::mul_k3(vChannels[0].vIn, in_buf[0], fInGain, to_process);
                else if (nMode == GM_MS)
                {
                    dsp::lr_to_ms(vChannels[0].vIn, vChannels[1].vIn, in_buf[0], in_buf[1], to_process);
                    dsp::mul_k2(vChannels[0].vIn, fInGain, to_process);
                    dsp::mul_k2(vChannels[1].vIn, fInGain, to_process);
                }
                else
                {
                    dsp::mul_k3(vChannels[0].vIn, in_buf[0], fInGain, to_process);
                    dsp::mul_k3(vChannels[1].vIn, in_buf[1], fInGain, to_process);
                }

                // Perform sidechain processing for each channel
                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c        = &vChannels[i];

                    // Update input graph
                    c->sGraph[G_IN].process(c->vIn, to_process);
                    c->pMeter[M_IN]->set_value(dsp::abs_max(c->vIn, to_process));

                    // Do gating
                    in[0]   = (c->nScType == SCT_EXTERNAL) ? sc_buf[0] : vChannels[0].vIn;
                    if (channels > 1)
                        in[1]   = (c->nScType == SCT_EXTERNAL) ? sc_buf[1] : vChannels[1].vIn;
                    c->sSC.process(c->vSc, in, to_process);
                    c->sGate.process(c->vGain, c->vEnv, c->vSc, to_process);

                    // Update gating dot
                    size_t idx = dsp::max_index(c->vEnv, to_process);
                    if (c->vEnv[idx] > c->fDotIn)
                    {
                        c->fDotIn   = c->vEnv[idx];
                        c->fDotOut  = c->vGain[idx] * c->fDotIn * c->fMakeup;
                    }
                }

                // Apply gain to each channel
                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c        = &vChannels[i];

                    // Add delay to original signal and apply gain
                    c->sLaDelay.process(c->vOut, c->vIn, c->vGain, to_process);
                    c->sInDelay.process(c->vIn, c->vIn, to_process);
                    c->sOutDelay.process(c->vOut, c->vOut, to_process);

                    // Process graph outputs
                    if ((i == 0) || (nMode != GM_STEREO))
                    {
                        c->sGraph[G_SC].process(c->vSc, to_process);                        // Sidechain signal
                        c->pMeter[M_SC]->set_value(dsp::abs_max(c->vSc, to_process));

                        c->sGraph[G_GAIN].process(c->vGain, to_process);                    // Gain reduction signal
                        c->pMeter[M_GAIN]->set_value(dsp::abs_max(c->vGain, to_process));

                        c->sGraph[G_ENV].process(c->vEnv, to_process);                      // Envelope signal
                        c->pMeter[M_ENV]->set_value(dsp::abs_max(c->vEnv, to_process));
                    }
                }

                // Form output signal
                if (nMode == GM_MS)
                {
                    channel_t *cm       = &vChannels[0];
                    channel_t *cs       = &vChannels[1];

                    dsp::mix2(cm->vOut, cm->vIn, cm->fMakeup * cm->fWetGain, cm->fDryGain, to_process);
                    dsp::mix2(cs->vOut, cs->vIn, cs->fMakeup * cs->fWetGain, cs->fDryGain, to_process);

                    cm->sGraph[G_OUT].process(cm->vOut, to_process);
                    cm->pMeter[M_OUT]->set_value(dsp::abs_max(cm->vOut, to_process));
                    cs->sGraph[G_OUT].process(cs->vOut, to_process);
                    cs->pMeter[M_OUT]->set_value(dsp::abs_max(cs->vOut, to_process));

                    if (!bMSListen)
                        dsp::ms_to_lr(cm->vOut, cs->vOut, cm->vOut, cs->vOut, to_process);
                    if (cm->bScListen)
                        dsp::copy(cm->vOut, cm->vSc, to_process);
                    if (cs->bScListen)
                        dsp::copy(cs->vOut, cs->vSc, to_process);
                }
                else
                {
                    for (size_t i=0; i<channels; ++i)
                    {
                        // Mix dry/wet signal or copy sidechain signal
                        channel_t *c        = &vChannels[i];
                        if (c->bScListen)
                            dsp::copy(c->vOut, c->vSc, to_process);
                        else
                            dsp::mix2(c->vOut, c->vIn, c->fMakeup * c->fWetGain, c->fDryGain, to_process);

                        c->sGraph[G_OUT].process(c->vOut, to_process);                      // Output signal
                        c->pMeter[M_OUT]->set_value(dsp::abs_max(c->vOut, to_process));
                    }
                }

                // Final metering
                for (size_t i=0; i<channels; ++i)
                {
                    // Apply bypass
                    channel_t *c        = &vChannels[i];
                    c->sDryDelay.process(c->vIn, in_buf[i], to_process);
                    c->sBypass.process(out_buf[i], c->vIn, c->vOut, to_process);

                    in_buf[i]          += to_process;
                    out_buf[i]         += to_process;
                    sc_buf[i]          += to_process;
                }

                left       -= to_process;
            }

            if ((!bPause) || (bClear) || (bUISync))
            {
                // Process mesh requests
                for (size_t i=0; i<channels; ++i)
                {
                    // Get channel
                    channel_t *c        = &vChannels[i];

                    for (size_t j=0; j<G_TOTAL; ++j)
                    {
                        // Check that port is bound
                        if (c->pGraph[j] == NULL)
                            continue;

                        // Clear data if requested
                        if (bClear)
                            dsp::fill_zero(c->sGraph[j].data(), meta::gate_metadata::TIME_MESH_SIZE);

                        // Get mesh
                        plug::mesh_t *mesh    = c->pGraph[j]->buffer<plug::mesh_t>();
                        if ((mesh != NULL) && (mesh->isEmpty()))
                        {
                            // Fill mesh with new values
                            dsp::copy(mesh->pvData[0], vTime, meta::gate_metadata::TIME_MESH_SIZE);
                            dsp::copy(mesh->pvData[1], c->sGraph[j].data(), meta::gate_metadata::TIME_MESH_SIZE);
                            mesh->data(2, meta::gate_metadata::TIME_MESH_SIZE);
                        }
                    } // for j
                }
                bUISync = false;
            }

            // Output gate curves for each channel
            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c       = &vChannels[i];

                // Output gate curves
                for (size_t j=0; j<2; ++j)
                {
                    if (c->pCurve[j] == NULL)
                        continue;

                    plug::mesh_t *mesh            = c->pCurve[j]->buffer<plug::mesh_t>();
                    if ((c->nSync & (S_CURVE << j)) && (mesh != NULL) && (mesh->isEmpty()))
                    {
                        // Copy frequency points
                        dsp::copy(mesh->pvData[0], vCurve, meta::gate_metadata::CURVE_MESH_SIZE);
                        c->sGate.curve(mesh->pvData[1], vCurve, meta::gate_metadata::CURVE_MESH_SIZE, j > 0);
                        if (c->fMakeup != 1.0f)
                            dsp::mul_k2(mesh->pvData[1], c->fMakeup, meta::gate_metadata::CURVE_MESH_SIZE);

                        // Mark mesh containing data
                        mesh->data(2, meta::gate_metadata::CURVE_MESH_SIZE);
                        c->nSync &= ~(S_CURVE << j);
                    }
                }

                // Update meter
                if ((c->pMeter[M_ENV] != NULL) && (c->pMeter[M_CURVE] != NULL))
                {
                    c->pMeter[M_ENV]->set_value(c->fDotIn);
                    c->pMeter[M_CURVE]->set_value(c->fDotOut);
                }
            }

            // Request for redraw
            if (pWrapper != NULL)
                pWrapper->query_display_draw();
        }

        bool gate::inline_display(plug::ICanvas *cv, size_t width, size_t height)
        {
            // Check proportions
            if (height > width)
                height  = width;

            // Init canvas
            if (!cv->init(width, height))
                return false;
            width   = cv->width();
            height  = cv->height();

            // Clear background
            bool bypassing = vChannels[0].sBypass.bypassing();
            cv->set_color_rgb((bypassing) ? CV_DISABLED : CV_BACKGROUND);
            cv->paint();

            float zx    = 1.0f/GAIN_AMP_M_72_DB;
            float zy    = 1.0f/GAIN_AMP_M_72_DB;
            float dx    = width/(logf(GAIN_AMP_P_24_DB)-logf(GAIN_AMP_M_72_DB));
            float dy    = height/(logf(GAIN_AMP_M_72_DB)-logf(GAIN_AMP_P_24_DB));

            // Draw horizontal and vertical lines
            cv->set_line_width(1.0);
            cv->set_color_rgb((bypassing) ? CV_SILVER: CV_YELLOW, 0.5f);
            for (float i=GAIN_AMP_M_72_DB; i<GAIN_AMP_P_24_DB; i *= GAIN_AMP_P_24_DB)
            {
                float ax = dx*(logf(i*zx));
                float ay = height + dy*(logf(i*zy));
                cv->line(ax, 0, ax, height);
                cv->line(0, ay, width, ay);
            }

            // Draw 1:1 line
            cv->set_line_width(2.0);
            cv->set_color_rgb(CV_GRAY);
            {
                float ax1 = dx*(logf(GAIN_AMP_M_72_DB*zx));
                float ax2 = dx*(logf(GAIN_AMP_P_24_DB*zx));
                float ay1 = height + dy*(logf(GAIN_AMP_M_72_DB*zy));
                float ay2 = height + dy*(logf(GAIN_AMP_P_24_DB*zy));
                cv->line(ax1, ay1, ax2, ay2);
            }

            // Draw axis
            cv->set_color_rgb((bypassing) ? CV_SILVER : CV_WHITE);
            {
                float ax = dx*(logf(GAIN_AMP_0_DB*zx));
                float ay = height + dy*(logf(GAIN_AMP_0_DB*zy));
                cv->line(ax, 0, ax, height);
                cv->line(0, ay, width, ay);
            }

            // Reuse display
            pIDisplay           = core::IDBuffer::reuse(pIDisplay, 4, width);
            core::IDBuffer *b   = pIDisplay;
            if (b == NULL)
                return false;

            size_t channels = ((nMode == GM_MONO) || (nMode == GM_STEREO)) ? 1 : 2;
            static uint32_t c_colors[] = {
                    CV_MIDDLE_CHANNEL, CV_MIDDLE_CHANNEL,
                    CV_MIDDLE_CHANNEL, CV_MIDDLE_CHANNEL,
                    CV_LEFT_CHANNEL, CV_RIGHT_CHANNEL,
                    CV_MIDDLE_CHANNEL, CV_SIDE_CHANNEL
                   };

            bool aa = cv->set_anti_aliasing(true);
            cv->set_line_width(2);

            for (size_t i=0; i<channels; ++i)
            {
                channel_t *c    = &vChannels[i];

                for (size_t j=0; j<2; ++j)
                {
                    for (size_t k=0; k<width; ++k)
                    {
                        size_t n        = (k*meta::gate_metadata::CURVE_MESH_SIZE)/width;
                        b->v[0][k]      = vCurve[n];
                    }
                    c->sGate.curve(b->v[1], b->v[0], width, j > 0);
                    if (c->fMakeup != 1.0f)
                        dsp::mul_k2(b->v[1], c->fMakeup, width);

                    dsp::fill(b->v[2], 0.0f, width);
                    dsp::fill(b->v[3], height, width);
                    dsp::axis_apply_log1(b->v[2], b->v[0], zx, dx, width);
                    dsp::axis_apply_log1(b->v[3], b->v[1], zy, dy, width);

                    // Draw mesh
                    uint32_t color = (bypassing || !(active())) ? CV_SILVER : c_colors[nMode*2 + i];
                    cv->set_color_rgb(color);
                    cv->draw_lines(b->v[2], b->v[3], width);
                }
            }

            // Draw dot
            if (active())
            {
                for (size_t i=0; i<channels; ++i)
                {
                    channel_t *c    = &vChannels[i];

                    uint32_t color = (bypassing) ? CV_SILVER : c_colors[nMode*2 + i];
                    Color c1(color), c2(color);
                    c2.alpha(0.9);

                    float ax = dx*(logf(c->fDotIn*zx));
                    float ay = height + dy*(logf(c->fDotOut*zy));

                    cv->radial_gradient(ax, ay, c1, c2, 12);
                    cv->set_color_rgb(0);
                    cv->circle(ax, ay, 4);
                    cv->set_color_rgb(color);
                    cv->circle(ax, ay, 3);
                }
            }

            cv->set_anti_aliasing(aa);

            return true;
        }

        void gate::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            size_t channels = (nMode == GM_MONO) ? 1 : 2;

            v->write("nMode", nMode);
            v->write("nChannels", channels);
            v->write("bSidechain", bSidechain);

            v->begin_array("vChannels", vChannels, channels);
            for (size_t i=0; i<channels; ++i)
            {
                const channel_t *c = &vChannels[i];

                v->begin_object(c, sizeof(channel_t));
                {
                    v->write_object("sBypass", &c->sBypass);
                    v->write_object("sSC", &c->sSC);
                    v->write_object("sSCEq", &c->sSCEq);
                    v->write_object("sGate", &c->sGate);
                    v->write_object("sLaDelay", &c->sLaDelay);
                    v->write_object("sInDelay", &c->sInDelay);
                    v->write_object("sOutDelay", &c->sOutDelay);
                    v->write_object("sDryDelay", &c->sDryDelay);
                    v->begin_array("sGraph", c->sGraph, G_TOTAL);
                    for (size_t j=0; j<G_TOTAL; ++j)
                        v->write_object(&c->sGraph[j]);
                    v->end_array();

                    v->write("vIn", c->vIn);
                    v->write("vOut", c->vOut);
                    v->write("vSc", c->vSc);
                    v->write("vEnv", c->vEnv);
                    v->write("vGain", c->vGain);
                    v->write("bScListen", c->bScListen);
                    v->write("nSync", c->nSync);
                    v->write("nScType", c->nScType);
                    v->write("fMakeup", c->fMakeup);
                    v->write("fDryGain", c->fDryGain);
                    v->write("fWetGain", c->fWetGain);
                    v->write("fDotIn", c->fDotIn);
                    v->write("fDotOut", c->fDotOut);

                    v->write("pIn", c->pIn);
                    v->write("pOut", c->pOut);
                    v->write("pSC", c->pSC);
                    v->begin_array("pGraph", c->pGraph, G_TOTAL);
                    for (size_t j=0; j<G_TOTAL; ++j)
                        v->write(c->pGraph[j]);
                    v->end_array();
                    v->begin_array("pMeter", c->pGraph, M_TOTAL);
                    for (size_t j=0; j<M_TOTAL; ++j)
                        v->write(c->pMeter[j]);
                    v->end_array();

                    v->write("pScType", c->pScType);
                    v->write("pScMode", c->pScMode);
                    v->write("pScLookahead", c->pScLookahead);
                    v->write("pScListen", c->pScListen);
                    v->write("pScSource", c->pScSource);
                    v->write("pScReactivity", c->pScReactivity);
                    v->write("pScPreamp", c->pScPreamp);
                    v->write("pScHpfMode", c->pScHpfMode);
                    v->write("pScHpfFreq", c->pScHpfFreq);
                    v->write("pScLpfMode", c->pScLpfMode);
                    v->write("pScLpfFreq", c->pScLpfFreq);

                    v->write("pHyst", c->pHyst);
                    v->writev("pThresh", c->pThresh, 2);
                    v->writev("pZone", c->pZone, 2);
                    v->write("pAttack", c->pAttack);
                    v->write("pRelease", c->pRelease);
                    v->write("pReduction", c->pReduction);
                    v->write("pMakeup", c->pMakeup);

                    v->write("pDryGain", c->pDryGain);
                    v->write("pWetGain", c->pWetGain);
                    v->writev("pCurve", c->pCurve, 2);
                    v->writev("pZoneStart", c->pZoneStart, 2);
                    v->write("pHystStart", c->pHystStart);
                }
                v->end_object();
            }
            v->end_array();

            v->write("vCurve", vCurve);
            v->write("vTime", vTime);
            v->write("bPause", bPause);
            v->write("bClear", bClear);
            v->write("bMSListen", bMSListen);
            v->write("fInGain", fInGain);
            v->write("bUISync", bUISync);
            v->write("pIDisplay", pIDisplay);

            v->write("pBypass", pBypass);
            v->write("pInGain", pInGain);
            v->write("pOutGain", pOutGain);
            v->write("pPause", pPause);
            v->write("pClear", pClear);
            v->write("pMSListen", pMSListen);

            v->write("pData", pData);
        }
    } // namespace plugins
} // namespace lsp


